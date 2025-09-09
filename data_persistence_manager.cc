#include "data_persistence_manager.hh"
#include "main/system.hh"
#include "main/check.hh"
#include "main/settings.hh"
#include "main/archive.hh"
#include "main/terminal.hh"
#include "main/manager.hh"
#include "main/remote_printer.hh"
#include "logger.hh"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>

// Static member definitions
std::unique_ptr<DataPersistenceManager> DataPersistenceManager::instance = nullptr;
std::mutex DataPersistenceManager::instance_mutex;

DataPersistenceManager::DataPersistenceManager()
    : system_ref(nullptr)
    , auto_save_interval(std::chrono::seconds(30))  // 30 seconds default
    , last_auto_save(std::chrono::steady_clock::now())
    , auto_save_enabled(true)
    , cups_communication_healthy(true)
    , last_cups_check(std::chrono::steady_clock::now())
    , cups_check_interval(std::chrono::seconds(60))  // Check CUPS every minute
    , shutdown_in_progress(false)
    , force_shutdown(false)
{
    FnTrace("DataPersistenceManager::DataPersistenceManager()");
}

DataPersistenceManager::~DataPersistenceManager()
{
    FnTrace("DataPersistenceManager::~DataPersistenceManager()");
    if (!shutdown_in_progress) {
        PrepareForShutdown();
    }
}

DataPersistenceManager& DataPersistenceManager::GetInstance()
{
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (!instance) {
        instance = std::unique_ptr<DataPersistenceManager>(new DataPersistenceManager());
    }
    return *instance;
}

void DataPersistenceManager::Initialize(System* system)
{
    FnTrace("DataPersistenceManager::Initialize()");
    auto& manager = GetInstance();
    manager.system_ref = system;
    
    // Register critical data validation and save callbacks
    manager.RegisterCriticalData("checks", 
        [&manager]() { return manager.ValidateChecks(); },
        [&manager]() { return manager.SaveAllChecks(); });
    
    manager.RegisterCriticalData("settings",
        [&manager]() { return manager.ValidateSettings(); },
        [&manager]() { return manager.SaveAllSettings(); });
    
    manager.RegisterCriticalData("archives",
        [&manager]() { return manager.ValidateArchives(); },
        [&manager]() { return manager.SaveAllArchives(); });
    
    manager.RegisterCriticalData("terminals",
        [&manager]() { return manager.ValidateTerminals(); },
        [&manager]() { return manager.SaveAllTerminals(); });
    
    manager.RegisterCriticalData("cups_communication",
        [&manager]() { return manager.ValidateCUPSCommunication(); },
        [&manager]() { return SaveResult::SAVE_SUCCESS; }); // CUPS doesn't need saving
    
    manager.LogInfo("DataPersistenceManager initialized successfully");
}

void DataPersistenceManager::Shutdown()
{
    FnTrace("DataPersistenceManager::Shutdown()");
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (instance) {
        instance->PrepareForShutdown();
        instance.reset();
    }
}

void DataPersistenceManager::SetAutoSaveInterval(std::chrono::seconds interval)
{
    auto_save_interval = interval;
    LogInfo("Auto-save interval set to " + std::to_string(interval.count()) + " seconds");
}

void DataPersistenceManager::EnableAutoSave(bool enable)
{
    auto_save_enabled = enable;
    LogInfo(std::string("Auto-save ") + (enable ? "enabled" : "disabled"));
}

void DataPersistenceManager::SetCUPSCheckInterval(std::chrono::seconds interval)
{
    cups_check_interval = interval;
    LogInfo("CUPS check interval set to " + std::to_string(interval.count()) + " seconds");
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateAllData()
{
    FnTrace("DataPersistenceManager::ValidateAllData()");
    
    ValidationResult overall_result = VALIDATION_SUCCESS;
    
    for (const auto& callback : validation_callbacks) {
        ValidationResult result = callback();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    for (const auto& data_item : critical_data_items) {
        ValidationResult result = data_item.validator();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    return overall_result;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateCriticalData()
{
    FnTrace("DataPersistenceManager::ValidateCriticalData()");
    
    ValidationResult overall_result = VALIDATION_SUCCESS;
    
    for (const auto& data_item : critical_data_items) {
        ValidationResult result = data_item.validator();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    return overall_result;
}

void DataPersistenceManager::RegisterValidationCallback(const std::string& name, ValidationCallback callback)
{
    validation_callbacks.push_back(callback);
    LogInfo("Registered validation callback: " + name);
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllData()
{
    FnTrace("DataPersistenceManager::SaveAllData()");
    
    SaveResult overall_result = SAVE_SUCCESS;
    
    // Save all registered data
    for (const auto& callback : save_callbacks) {
        SaveResult result = callback();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    for (const auto& data_item : critical_data_items) {
        SaveResult result = data_item.saver();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    return overall_result;
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveCriticalData()
{
    FnTrace("DataPersistenceManager::SaveCriticalData()");
    
    SaveResult overall_result = SAVE_SUCCESS;
    
    for (const auto& data_item : critical_data_items) {
        SaveResult result = data_item.saver();
        if (result > overall_result) {
            overall_result = result;
        }
    }
    
    return overall_result;
}

void DataPersistenceManager::RegisterSaveCallback(const std::string& name, SaveCallback callback)
{
    save_callbacks.push_back(callback);
    LogInfo("Registered save callback: " + name);
}

void DataPersistenceManager::RegisterCriticalData(const std::string& name, 
                                                 ValidationCallback validator, 
                                                 SaveCallback saver)
{
    CriticalData data_item;
    data_item.name = name;
    data_item.is_dirty = false;
    data_item.last_modified = std::chrono::steady_clock::now();
    data_item.validator = validator;
    data_item.saver = saver;
    
    critical_data_items.push_back(data_item);
    LogInfo("Registered critical data: " + name);
}

void DataPersistenceManager::MarkDataDirty(const std::string& name)
{
    for (auto& data_item : critical_data_items) {
        if (data_item.name == name) {
            data_item.is_dirty = true;
            data_item.last_modified = std::chrono::steady_clock::now();
            break;
        }
    }
}

void DataPersistenceManager::MarkDataClean(const std::string& name)
{
    for (auto& data_item : critical_data_items) {
        if (data_item.name == name) {
            data_item.is_dirty = false;
            break;
        }
    }
}

bool DataPersistenceManager::IsDataDirty(const std::string& name) const
{
    for (const auto& data_item : critical_data_items) {
        if (data_item.name == name) {
            return data_item.is_dirty;
        }
    }
    return false;
}

bool DataPersistenceManager::IsCUPSHealthy() const
{
    return cups_communication_healthy;
}

void DataPersistenceManager::CheckCUPSStatus()
{
    FnTrace("DataPersistenceManager::CheckCUPSStatus()");
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_cups_check);
    
    if (elapsed >= cups_check_interval) {
        cups_communication_healthy = CheckCUPSHealth();
        last_cups_check = now;
        
        if (!cups_communication_healthy) {
            LogWarning("CUPS communication unhealthy - attempting recovery");
            AttemptCUPSRecovery();
        }
    }
}

void DataPersistenceManager::ForceCUPSRecovery()
{
    FnTrace("DataPersistenceManager::ForceCUPSRecovery()");
    AttemptCUPSRecovery();
}

void DataPersistenceManager::ProcessPeriodicTasks()
{
    FnTrace("DataPersistenceManager::ProcessPeriodicTasks()");
    
    auto now = std::chrono::steady_clock::now();
    
    // Check if auto-save is needed
    if (auto_save_enabled) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_auto_save);
        if (elapsed >= auto_save_interval) {
            LogInfo("Performing periodic auto-save");
            SaveResult result = SaveCriticalData();
            if (result == SAVE_SUCCESS) {
                last_auto_save = now;
                LogInfo("Auto-save completed successfully");
            } else {
                LogError("Auto-save failed with result: " + std::to_string(result));
            }
        }
    }
    
    // Check CUPS status
    CheckCUPSStatus();
}

void DataPersistenceManager::Update()
{
    ProcessPeriodicTasks();
}

DataPersistenceManager::SaveResult DataPersistenceManager::PrepareForShutdown()
{
    FnTrace("DataPersistenceManager::PrepareForShutdown()");
    
    if (shutdown_in_progress) {
        LogWarning("Shutdown already in progress");
        return SAVE_SUCCESS;
    }
    
    shutdown_in_progress = true;
    LogInfo("Preparing for system shutdown - validating and saving all data");
    
    // First, validate all data
    ValidationResult validation_result = ValidateAllData();
    if (validation_result >= VALIDATION_ERROR) {
        LogError("Data validation failed before shutdown - some data may be corrupted");
        if (validation_result == VALIDATION_CRITICAL) {
            LogError("Critical data validation failure - forcing emergency save");
            EmergencySave();
        }
    }
    
    // Save all data
    SaveResult save_result = SaveAllData();
    if (save_result != SAVE_SUCCESS) {
        LogError("Data save failed during shutdown preparation");
        if (save_result == SAVE_CRITICAL_FAILURE) {
            LogError("Critical save failure - attempting emergency save");
            EmergencySave();
        }
    }
    
    // Create backup before shutdown
    CreateBackup();
    
    LogInfo("Shutdown preparation completed with result: " + std::to_string(save_result));
    return save_result;
}

DataPersistenceManager::SaveResult DataPersistenceManager::ForceShutdown()
{
    FnTrace("DataPersistenceManager::ForceShutdown()");
    
    force_shutdown = true;
    LogWarning("Force shutdown requested - performing emergency save");
    
    EmergencySave();
    return SAVE_SUCCESS; // Always return success for force shutdown
}

bool DataPersistenceManager::CanSafelyShutdown() const
{
    // Check if any critical data is dirty
    for (const auto& data_item : critical_data_items) {
        if (data_item.is_dirty) {
            return false;
        }
    }
    
    // Check if CUPS is healthy
    if (!cups_communication_healthy) {
        return false;
    }
    
    return true;
}

std::vector<std::string> DataPersistenceManager::GetErrorLog() const
{
    std::lock_guard<std::mutex> lock(log_mutex);
    return error_log;
}

std::vector<std::string> DataPersistenceManager::GetWarningLog() const
{
    std::lock_guard<std::mutex> lock(log_mutex);
    return warning_log;
}

void DataPersistenceManager::ClearLogs()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    error_log.clear();
    warning_log.clear();
}

std::string DataPersistenceManager::GenerateIntegrityReport() const
{
    std::ostringstream report;
    report << "=== Data Integrity Report ===\n";
    report << "Timestamp: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    report << "CUPS Communication: " << (cups_communication_healthy ? "Healthy" : "Unhealthy") << "\n";
    report << "Auto-save Enabled: " << (auto_save_enabled ? "Yes" : "No") << "\n";
    report << "Shutdown in Progress: " << (shutdown_in_progress ? "Yes" : "No") << "\n\n";
    
    report << "Critical Data Status:\n";
    for (const auto& data_item : critical_data_items) {
        report << "  " << data_item.name << ": " 
               << (data_item.is_dirty ? "Dirty" : "Clean") << "\n";
    }
    
    report << "\nError Count: " << error_log.size() << "\n";
    report << "Warning Count: " << warning_log.size() << "\n";
    
    return report.str();
}

bool DataPersistenceManager::HasDataIntegrityIssues() const
{
    // Check for dirty data
    for (const auto& data_item : critical_data_items) {
        if (data_item.is_dirty) {
            return true;
        }
    }
    
    // Check for CUPS issues
    if (!cups_communication_healthy) {
        return true;
    }
    
    // Check for recent errors
    if (!error_log.empty()) {
        return true;
    }
    
    return false;
}

void DataPersistenceManager::EmergencySave()
{
    FnTrace("DataPersistenceManager::EmergencySave()");
    
    LogWarning("Performing emergency save of critical data");
    
    // Save only the most critical data
    SaveAllChecks();
    SaveAllSettings();
    
    LogInfo("Emergency save completed");
}

void DataPersistenceManager::CreateBackup()
{
    FnTrace("DataPersistenceManager::CreateBackup()");
    
    if (!system_ref) {
        LogError("Cannot create backup - system reference is null");
        return;
    }
    
    // Create backup directory with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_val = std::chrono::system_clock::to_time_t(now);
    std::ostringstream backup_dir;
    backup_dir << "/tmp/viewtouch_backup_" << time_val;
    
    try {
        std::filesystem::create_directories(backup_dir.str());
        
        // Copy critical files
        std::string data_path = system_ref->data_path.str();
        std::string backup_cmd = "cp -r " + data_path + "/* " + backup_dir.str() + "/";
        
        int result = system(backup_cmd.c_str());
        if (result == 0) {
            LogInfo("Backup created successfully: " + backup_dir.str());
        } else {
            LogError("Backup creation failed with exit code: " + std::to_string(result));
        }
    } catch (const std::exception& e) {
        LogError("Exception during backup creation: " + std::string(e.what()));
    }
}

bool DataPersistenceManager::RestoreFromBackup(const std::string& backup_path)
{
    FnTrace("DataPersistenceManager::RestoreFromBackup()");
    
    if (!std::filesystem::exists(backup_path)) {
        LogError("Backup path does not exist: " + backup_path);
        return false;
    }
    
    if (!system_ref) {
        LogError("Cannot restore backup - system reference is null");
        return false;
    }
    
    try {
        std::string data_path = system_ref->data_path.str();
        std::string restore_cmd = "cp -r " + backup_path + "/* " + data_path + "/";
        
        int result = system(restore_cmd.c_str());
        if (result == 0) {
            LogInfo("Backup restored successfully from: " + backup_path);
            return true;
        } else {
            LogError("Backup restore failed with exit code: " + std::to_string(result));
            return false;
        }
    } catch (const std::exception& e) {
        LogError("Exception during backup restore: " + std::string(e.what()));
        return false;
    }
}

// Internal validation methods
DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateChecks()
{
    FnTrace("DataPersistenceManager::ValidateChecks()");
    
    if (!system_ref) {
        LogError("Cannot validate checks - system reference is null");
        return VALIDATION_ERROR;
    }
    
    int check_count = 0;
    int valid_checks = 0;
    
    Check* check = system_ref->CheckList();
    while (check != nullptr) {
        check_count++;
        
        // Basic validation
        if (check->serial_number > 0 && !check->filename.empty()) {
            valid_checks++;
        } else {
            LogWarning("Invalid check found: serial=" + std::to_string(check->serial_number) + 
                      ", filename=" + check->filename.str());
        }
        
        check = check->next;
    }
    
    if (check_count == 0) {
        return VALIDATION_SUCCESS;
    }
    
    double validity_ratio = static_cast<double>(valid_checks) / check_count;
    if (validity_ratio >= 0.95) {
        return VALIDATION_SUCCESS;
    } else if (validity_ratio >= 0.80) {
        return VALIDATION_WARNING;
    } else {
        return VALIDATION_ERROR;
    }
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateSettings()
{
    FnTrace("DataPersistenceManager::ValidateSettings()");
    
    if (!system_ref) {
        LogError("Cannot validate settings - system reference is null");
        return VALIDATION_ERROR;
    }
    
    Settings* settings = &system_ref->settings;
    if (!settings) {
        LogError("Settings object is null");
        return VALIDATION_ERROR;
    }
    
    // Basic settings validation
    if (settings->store_name.empty()) {
        LogWarning("Store name is empty");
        return VALIDATION_WARNING;
    }
    
    return VALIDATION_SUCCESS;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateArchives()
{
    FnTrace("DataPersistenceManager::ValidateArchives()");
    
    if (!system_ref) {
        LogError("Cannot validate archives - system reference is null");
        return VALIDATION_ERROR;
    }
    
    // Basic archive validation
    Archive* archive = system_ref->ArchiveList();
    int archive_count = 0;
    
    while (archive != nullptr) {
        archive_count++;
        archive = archive->next;
    }
    
    return VALIDATION_SUCCESS;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateTerminals()
{
    FnTrace("DataPersistenceManager::ValidateTerminals()");
    
    if (!MasterControl) {
        LogError("Cannot validate terminals - MasterControl is null");
        return VALIDATION_ERROR;
    }
    
    Terminal* term = MasterControl->TermList();
    int terminal_count = 0;
    
    while (term != nullptr) {
        terminal_count++;
        term = term->next;
    }
    
    return VALIDATION_SUCCESS;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateCUPSCommunication()
{
    FnTrace("DataPersistenceManager::ValidateCUPSCommunication()");
    
    return CheckCUPSHealth() ? VALIDATION_SUCCESS : VALIDATION_ERROR;
}

// Internal save methods
DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllChecks()
{
    FnTrace("DataPersistenceManager::SaveAllChecks()");
    
    if (!system_ref) {
        LogError("Cannot save checks - system reference is null");
        return SAVE_FAILED;
    }
    
    int saved_count = 0;
    int total_count = 0;
    
    Check* check = system_ref->CheckList();
    while (check != nullptr) {
        total_count++;
        
        if (check->Save() == 0) {
            saved_count++;
        } else {
            LogError("Failed to save check with serial number: " + std::to_string(check->serial_number));
        }
        
        check = check->next;
    }
    
    if (total_count == 0) {
        return SAVE_SUCCESS;
    }
    
    double save_ratio = static_cast<double>(saved_count) / total_count;
    if (save_ratio >= 0.95) {
        return SAVE_SUCCESS;
    } else if (save_ratio >= 0.80) {
        return SAVE_PARTIAL;
    } else {
        return SAVE_FAILED;
    }
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllSettings()
{
    FnTrace("DataPersistenceManager::SaveAllSettings()");
    
    if (!system_ref) {
        LogError("Cannot save settings - system reference is null");
        return SAVE_FAILED;
    }
    
    Settings* settings = &system_ref->settings;
    if (!settings) {
        LogError("Settings object is null");
        return SAVE_FAILED;
    }
    
    if (settings->Save() == 0) {
        return SAVE_SUCCESS;
    } else {
        LogError("Failed to save settings");
        return SAVE_FAILED;
    }
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllArchives()
{
    FnTrace("DataPersistenceManager::SaveAllArchives()");
    
    if (!system_ref) {
        LogError("Cannot save archives - system reference is null");
        return SAVE_FAILED;
    }
    
    return system_ref->SaveChanged() == 0 ? SAVE_SUCCESS : SAVE_FAILED;
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllTerminals()
{
    FnTrace("DataPersistenceManager::SaveAllTerminals()");
    
    if (!MasterControl) {
        LogError("Cannot save terminals - MasterControl is null");
        return SAVE_FAILED;
    }
    
    // Save any pending terminal changes
    Terminal* term = MasterControl->TermList();
    while (term != nullptr) {
        if (term->edit > 0) {
            term->EditTerm(1); // Save changes and exit edit mode
        }
        term = term->next;
    }
    
    return SAVE_SUCCESS;
}

// CUPS monitoring methods
bool DataPersistenceManager::CheckCUPSHealth()
{
    FnTrace("DataPersistenceManager::CheckCUPSHealth()");
    
    // Check if CUPS daemon is running
    int result = system("systemctl is-active --quiet cups");
    if (result != 0) {
        LogWarning("CUPS daemon is not running");
        return false;
    }
    
    // Check if we can communicate with CUPS
    result = system("lpstat -r > /dev/null 2>&1");
    if (result != 0) {
        LogWarning("Cannot communicate with CUPS (lpstat failed)");
        return false;
    }
    
    return true;
}

void DataPersistenceManager::AttemptCUPSRecovery()
{
    FnTrace("DataPersistenceManager::AttemptCUPSRecovery()");
    
    LogInfo("Attempting CUPS recovery");
    
    // Try to restart CUPS service
    int result = system("systemctl restart cups");
    if (result == 0) {
        LogInfo("CUPS service restarted successfully");
        
        // Wait a moment for service to start
        sleep(2);
        
        // Check if recovery was successful
        if (CheckCUPSHealth()) {
            cups_communication_healthy = true;
            LogInfo("CUPS recovery successful");
        } else {
            LogError("CUPS recovery failed - service restarted but still unhealthy");
        }
    } else {
        LogError("Failed to restart CUPS service");
    }
}

// Logging methods
void DataPersistenceManager::LogError(const std::string& message)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    error_log.push_back(message);
    ReportError(message.c_str());
}

void DataPersistenceManager::LogWarning(const std::string& message)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    warning_log.push_back(message);
    ReportError(message.c_str());
}

void DataPersistenceManager::LogInfo(const std::string& message)
{
    ReportError(message.c_str());
}

// Global convenience functions
DataPersistenceManager& GetDataPersistenceManager()
{
    return DataPersistenceManager::GetInstance();
}

void InitializeDataPersistence(System* system)
{
    DataPersistenceManager::Initialize(system);
}

void ShutdownDataPersistence()
{
    DataPersistenceManager::Shutdown();
}
