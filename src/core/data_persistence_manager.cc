#include "data_persistence_manager.hh"
#include "main/data/system.hh"
#include "main/business/check.hh"
#include "main/data/settings.hh"
#include "main/data/archive.hh"
#include "main/hardware/terminal.hh"
#include "main/data/manager.hh"
#include "main/hardware/remote_printer.hh"
#include "logger.hh"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

// Static member definitions
std::unique_ptr<DataPersistenceManager> DataPersistenceManager::instance = nullptr;
std::mutex DataPersistenceManager::instance_mutex;

// Helper function for safe command execution with timeout
static int ExecuteCommandWithTimeout(const std::string& command, std::chrono::seconds timeout)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1; // Pipe creation failed
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1; // Fork failed
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close read end
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127); // exec failed
    } else { // Parent process
        close(pipefd[1]); // Close write end

        // Set up timeout
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + timeout;

        int status;
        bool timed_out = false;

        while (std::chrono::steady_clock::now() < end_time) {
            if (waitpid(pid, &status, WNOHANG) > 0) {
                break;
            }
            usleep(10000); // Sleep 10ms
        }

        if (std::chrono::steady_clock::now() >= end_time) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timed_out = true;
        }

        close(pipefd[0]);
        return timed_out ? -2 : (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

// Helper function for recursive directory copy using system commands as fallback
static bool CopyDirectoryRecursively(const std::filesystem::path& source,
                                   const std::filesystem::path& destination)
{
    try {
        // Use system cp command for reliability across different C++ stdlib versions
        std::string cmd = "cp -r \"" + source.string() + "\"/* \"" + destination.string() + "/\" 2>/dev/null || true";
        int result = system(cmd.c_str());
        return result == 0;
    } catch (const std::exception&) {
        return false;
    }
}

DataPersistenceManager::DataPersistenceManager()
    : system_ref(nullptr)
    , last_auto_save(std::chrono::steady_clock::now())
    , cups_communication_healthy(true)
    , last_cups_check(std::chrono::steady_clock::now())
    , cups_consecutive_failures(0)
    , shutdown_in_progress(false)
    , force_shutdown(false)
{
    FnTrace("DataPersistenceManager::DataPersistenceManager()");

    // Initialize configuration with defaults
    config = Configuration();

    // Reserve space for logs to reduce reallocations
    error_log.reserve(static_cast<std::size_t>(config.max_error_log_size));
    warning_log.reserve(static_cast<std::size_t>(config.max_warning_log_size));
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
        []() { return SaveResult::SAVE_SUCCESS; }); // CUPS doesn't need saving
    
    manager.LogInfo("DataPersistenceManager initialized successfully");
}

void DataPersistenceManager::Shutdown()
{
    FnTrace("DataPersistenceManager::Shutdown()");
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (instance) {
        // Don't call PrepareForShutdown() again if shutdown is already in progress
        // This prevents potential deadlocks and duplicate shutdown operations
        if (!instance->shutdown_in_progress) {
            instance->PrepareForShutdown();
        }
        instance.reset();
    }
}

void DataPersistenceManager::SetAutoSaveInterval(std::chrono::seconds interval)
{
    config.auto_save_interval = interval;
    LogInfo("Auto-save interval set to " + std::to_string(interval.count()) + " seconds");
}

void DataPersistenceManager::EnableAutoSave(bool enable)
{
    config.enable_auto_save = enable;
    LogInfo(std::string("Auto-save ") + (enable ? "enabled" : "disabled"));
}

void DataPersistenceManager::SetCUPSCheckInterval(std::chrono::seconds interval)
{
    config.cups_check_interval = interval;
    LogInfo("CUPS check interval set to " + std::to_string(interval.count()) + " seconds");
}

void DataPersistenceManager::SetConfiguration(const Configuration& new_config)
{
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        config = new_config;
    }

    // Resize log vectors if needed (requires log_mutex)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        const auto max_error_size = static_cast<std::size_t>(config.max_error_log_size);
        const auto max_warning_size = static_cast<std::size_t>(config.max_warning_log_size);

        error_log.reserve(max_error_size);
        warning_log.reserve(max_warning_size);

        // Trim existing logs if they exceed new limits
        if (error_log.size() > max_error_size) {
            const auto excess = error_log.size() - max_error_size;
            error_log.erase(error_log.begin(),
                            error_log.begin() + static_cast<std::ptrdiff_t>(excess));
        }
        if (warning_log.size() > max_warning_size) {
            const auto excess = warning_log.size() - max_warning_size;
            warning_log.erase(warning_log.begin(),
                              warning_log.begin() + static_cast<std::ptrdiff_t>(excess));
        }
    }

    LogInfo("Configuration updated");
}

const DataPersistenceManager::Configuration& DataPersistenceManager::GetConfiguration() const
{
    // Note: Since config is read-only after initialization and the class is a singleton,
    // we don't strictly need to lock here for thread safety, but it's safer to do so
    // in case future changes make config mutable.
    std::lock_guard<std::mutex> lock(config_mutex);
    return config;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateAllData()
{
    FnTrace("DataPersistenceManager::ValidateAllData()");

    // Skip validation during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping data validation during shutdown to prevent hanging");
        return VALIDATION_SUCCESS;
    }

    auto start_time = std::chrono::steady_clock::now();
    ValidationResult overall_result = VALIDATION_SUCCESS;
    int failed_validations = 0;

    // Validate registered callbacks
    for (const auto& callback : validation_callbacks) {
        ValidationResult result = callback();
        if (result > overall_result) {
            overall_result = result;
        }
        if (result != VALIDATION_SUCCESS) {
            failed_validations++;
        }
    }

    // Validate critical data items
    for (const auto& data_item : critical_data_items) {
        ValidationResult result = data_item.validator();
        if (result > overall_result) {
            overall_result = result;
        }
        if (result != VALIDATION_SUCCESS) {
            failed_validations++;
        }
    }

    // Update metrics
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    metrics.total_validations++;
    metrics.failed_validations += failed_validations;
    metrics.total_validation_time += duration;

    return overall_result;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateCriticalData()
{
    FnTrace("DataPersistenceManager::ValidateCriticalData()");
    
    // Skip validation during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping critical data validation during shutdown to prevent hanging");
        return VALIDATION_SUCCESS;
    }
    
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
    
    // Skip saving during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping data save during shutdown to prevent hanging");
        return SAVE_SUCCESS;
    }
    
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
    
    // Skip saving during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping critical data save during shutdown to prevent hanging");
        return SAVE_SUCCESS;
    }
    
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
    data_item.consecutive_failures = 0;
    data_item.last_failure = std::chrono::steady_clock::now();

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
    
    // Skip CUPS checks during shutdown to prevent hanging
    if (shutdown_in_progress) {
        LogInfo("Skipping CUPS status check during shutdown to prevent hanging");
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_cups_check);

    if (elapsed >= config.cups_check_interval && config.enable_cups_monitoring) {
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
    
    // Skip CUPS recovery during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping CUPS recovery during shutdown to prevent hanging");
        return;
    }
    
    AttemptCUPSRecovery();
}

void DataPersistenceManager::ProcessPeriodicTasks()
{
    FnTrace("DataPersistenceManager::ProcessPeriodicTasks()");
    
    // Skip all periodic tasks during shutdown to prevent hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping periodic tasks during shutdown to prevent hanging");
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    
    // Check if auto-save is needed
    if (config.enable_auto_save) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_auto_save);
        if (elapsed >= config.auto_save_interval) {
            // Check if there's actually dirty data to save
            if (IsDataDirty("checks") || IsDataDirty("settings") || IsDataDirty("archives")) {
                // Skip auto-save if any terminal is in edit mode to avoid interrupting user workflow
                if (IsAnyTerminalInEditMode()) {
                    LogInfo("Skipping auto-save - terminal in edit mode (data is dirty)");
                } else {
                    LogInfo("Performing periodic auto-save (dirty data detected)");
                    SaveResult result = SaveCriticalData();
                    if (result == SAVE_SUCCESS) {
                        last_auto_save = now;
                        LogInfo("Auto-save completed successfully");
                    } else {
                        LogError("Auto-save failed with result: " + std::to_string(result), "auto_save");
                    }
                }
            } else {
                // No dirty data, just update the timestamp to avoid constant checking
                last_auto_save = now;
                LogInfo("Auto-save skipped - no dirty data");
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
    
    shutdown_in_progress.store(true, std::memory_order_release);
    LogInfo("Preparing for system shutdown - performing minimal cleanup only");
    
    // Force exit from edit mode during shutdown to ensure all changes are saved
    if (MasterControl) {
        Terminal* term = MasterControl->TermList();
        while (term != nullptr) {
            if (term->edit > 0) {
                LogInfo("Forcing exit from edit mode during shutdown for terminal");
                term->EditTerm(1); // Save changes and exit edit mode
            }
            term = term->next;
        }
    }
    
    // Skip all validation and saving operations during shutdown to prevent hanging
    // The system will rely on the normal save operations that happen during regular operation
    LogInfo("Skipping data validation and saving during shutdown to prevent hanging");
    
    LogInfo("Shutdown preparation completed - minimal cleanup only");
    return SAVE_SUCCESS;
}

DataPersistenceManager::SaveResult DataPersistenceManager::ForceShutdown()
{
    FnTrace("DataPersistenceManager::ForceShutdown()");
    
    force_shutdown.store(true, std::memory_order_release);
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
    std::vector<std::string> result;
    result.reserve(error_log.size());
    for (const auto& entry : error_log) {
        result.push_back(entry.message);
    }
    return result;
}

std::vector<std::string> DataPersistenceManager::GetWarningLog() const
{
    std::lock_guard<std::mutex> lock(log_mutex);
    std::vector<std::string> result;
    result.reserve(warning_log.size());
    for (const auto& entry : warning_log) {
        result.push_back(entry.message);
    }
    return result;
}

std::vector<DataPersistenceManager::ErrorInfo> DataPersistenceManager::GetDetailedErrorLog() const
{
    std::lock_guard<std::mutex> lock(log_mutex);
    return error_log;
}

std::vector<DataPersistenceManager::ErrorInfo> DataPersistenceManager::GetDetailedWarningLog() const
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

bool DataPersistenceManager::IsAnyTerminalInEditMode() const
{
    FnTrace("DataPersistenceManager::IsAnyTerminalInEditMode()");
    
    if (!MasterControl) {
        return false;
    }
    
    Terminal* term = MasterControl->TermList();
    while (term != nullptr) {
        if (term->edit > 0) {
            return true;
        }
        term = term->next;
    }
    
    return false;
}

std::string DataPersistenceManager::GenerateIntegrityReport() const
{
    std::ostringstream report;
    report << "=== Data Integrity Report ===\n";
    report << "Timestamp: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    report << "CUPS Communication: " << (cups_communication_healthy.load(std::memory_order_acquire) ? "Healthy" : "Unhealthy") << "\n";
    report << "CUPS Consecutive Failures: " << cups_consecutive_failures.load(std::memory_order_acquire) << "\n";
    report << "Auto-save Enabled: " << (config.enable_auto_save ? "Yes" : "No") << "\n";
    report << "Shutdown in Progress: " << (shutdown_in_progress.load(std::memory_order_acquire) ? "Yes" : "No") << "\n\n";

    report << "Critical Data Status:\n";
    for (const auto& data_item : critical_data_items) {
        report << "  " << data_item.name << ": "
               << (data_item.is_dirty ? "Dirty" : "Clean")
               << " (failures: " << data_item.consecutive_failures << ")\n";
    }

    report << "\nData Consistency: " << (VerifyDataConsistency() ? "OK" : "ISSUES") << "\n";

    report << "\nData Checksums:\n";
    report << "  Checks: " << GenerateDataChecksum("checks") << "\n";
    report << "  Settings: " << GenerateDataChecksum("settings") << "\n";
    report << "  Terminals: " << GenerateDataChecksum("terminals") << "\n";

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        report << "\nError Count: " << error_log.size() << "\n";
        report << "Warning Count: " << warning_log.size() << "\n";

        if (!error_log.empty()) {
            report << "\nRecent Errors:\n";
            int count = 0;
            for (auto it = error_log.rbegin(); it != error_log.rend() && count < 5; ++it, ++count) {
                report << "  " << it->message << "\n";
            }
        }
    }

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
    if (!cups_communication_healthy.load(std::memory_order_acquire)) {
        return true;
    }

    // Check for recent errors
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (!error_log.empty()) {
            return true;
        }
    }

    // Check for excessive consecutive failures
    if (cups_consecutive_failures.load(std::memory_order_acquire) > 5) {
        return true;
    }

    return false;
}

bool DataPersistenceManager::VerifyFileIntegrity(const std::string& file_path) const
{
    // Use stat() for basic file checks to avoid filesystem library issues
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        return false; // File doesn't exist or can't be accessed
    }

    // Check if it's a regular file
    if (!S_ISREG(file_stat.st_mode)) {
        return false;
    }

    // Check file size (not empty and not too large)
    if (file_stat.st_size <= 0 || file_stat.st_size > 100LL * 1024 * 1024) { // 100MB limit
        return false;
    }

    // Try to open and read first few bytes
    FILE* file = fopen(file_path.c_str(), "rb");
    if (!file) {
        return false;
    }

    char buffer[256];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    int eof_reached = feof(file);
    fclose(file);

    // File is readable if we could read at least 1 byte or reached EOF
    return bytes_read > 0 || eof_reached;
}

bool DataPersistenceManager::VerifyDataConsistency() const
{
    if (!system_ref) {
        return false;
    }

    bool consistent = true;

    // Check check data consistency (skip training checks)
    Check* check = system_ref->CheckList();
    while (check != nullptr) {
        // Skip training checks for consistency validation
        if (!check->IsTraining()) {
            // Verify check has valid references
            if (check->serial_number <= 0) {
                consistent = false;
                break;
            }
        }
        check = check->next;
    }

    // Check terminal consistency if MasterControl exists
    if (MasterControl) {
        Terminal* term = MasterControl->TermList();
        while (term != nullptr) {
            // Basic terminal validation
            if (term->edit < 0) {
                consistent = false;
                break;
            }
            term = term->next;
        }
    }

    return consistent;
}

std::string DataPersistenceManager::GenerateDataChecksum(const std::string& data_type) const
{
    // Simple checksum generation for data integrity monitoring
    // In a real implementation, this would use cryptographic hashes
    std::ostringstream checksum;

    if (data_type == "checks" && system_ref) {
        int count = 0;
        int training_count = 0;
        Check* check = system_ref->CheckList();
        while (check != nullptr) {
            if (check->IsTraining()) {
                training_count++;
            } else {
                count++;
            }
            check = check->next;
        }
        checksum << "checks:" << count << ":training:" << training_count;
    } else if (data_type == "settings" && system_ref) {
        checksum << "settings:" << system_ref->settings.store_name.size();
    } else if (data_type == "terminals" && MasterControl) {
        int count = 0;
        Terminal* term = MasterControl->TermList();
        while (term != nullptr) {
            count++;
            term = term->next;
        }
        checksum << "terminals:" << count;
    } else {
        checksum << "unknown:0";
    }

    return checksum.str();
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
        LogError("Cannot create backup - system reference is null", "backup");
        return;
    }

    // Create backup directory with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_val = std::chrono::system_clock::to_time_t(now);
    std::ostringstream backup_dir;
    backup_dir << config.backup_directory << "/viewtouch_backup_" << time_val;

    try {
        // Create backup directory using mkdir
        std::string mkdir_cmd = "mkdir -p \"" + backup_dir.str() + "\"";
        int mkdir_result = system(mkdir_cmd.c_str());

        if (mkdir_result != 0) {
            LogError("Failed to create backup directory: " + backup_dir.str(), "backup");
            return;
        }

        // Copy critical files using system cp
        std::string data_path = system_ref->data_path.str();

        if (CopyDirectoryRecursively(data_path, backup_dir.str())) {
            LogInfo("Backup created successfully: " + backup_dir.str());
        } else {
            LogError("Backup creation failed: could not copy files", "backup");
        }
    } catch (const std::exception& e) {
        LogError("Exception during backup creation: " + std::string(e.what()), "backup");
    }
}

bool DataPersistenceManager::RestoreFromBackup(const std::string& backup_path)
{
    FnTrace("DataPersistenceManager::RestoreFromBackup()");
    
    if (!std::filesystem::exists(backup_path)) {
        LogError("Backup path does not exist: " + backup_path, "backup");
        return false;
    }

    if (!system_ref) {
        LogError("Cannot restore backup - system reference is null", "backup");
        return false;
    }
    
    try {
        std::string data_path = system_ref->data_path.str();

        if (CopyDirectoryRecursively(backup_path, data_path)) {
            LogInfo("Backup restored successfully from: " + backup_path);
            return true;
        } else {
            LogError("Backup restore failed: could not copy files", "backup");
            return false;
        }
    } catch (const std::exception& e) {
        LogError("Exception during backup restore: " + std::string(e.what()), "backup");
        return false;
    }
}

// Internal validation methods
DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateChecks()
{
    FnTrace("DataPersistenceManager::ValidateChecks()");

    if (!system_ref) {
        LogError("Cannot validate checks - system reference is null", "validation");
        return VALIDATION_ERROR;
    }

    int check_count = 0;
    int valid_checks = 0;
    int invalid_checks = 0;

    // Single pass through the linked list for efficiency
    Check* check = system_ref->CheckList();
    while (check != nullptr) {
        check_count++;

        // Skip training checks - we don't validate training data
        if (check->IsTraining()) {
            check = check->next;
            continue;
        }

        // Basic validation - check for required fields
        bool is_valid = (check->serial_number > 0 && !check->filename.empty());
        if (is_valid) {
            valid_checks++;
        } else {
            invalid_checks++;
            // Only log first few invalid checks to avoid log spam
            if (invalid_checks <= 5) {
                LogWarning("Invalid check found: serial=" + std::to_string(check->serial_number) +
                          ", filename=" + check->filename.str(), "validation");
            }
        }

        check = check->next;
    }

    // Handle empty check list
    if (check_count == 0) {
        return VALIDATION_SUCCESS;
    }

    // Log summary if there were many invalid checks
    if (invalid_checks > 5) {
        LogWarning("Found " + std::to_string(invalid_checks) + " invalid checks out of " +
                  std::to_string(check_count) + " total", "validation");
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
        LogError("Cannot validate settings - system reference is null", "validation");
        return VALIDATION_ERROR;
    }

    Settings* settings = &system_ref->settings;
    if (!settings) {
        LogError("Settings object is null", "validation");
        return VALIDATION_ERROR;
    }

    // Basic settings validation
    if (settings->store_name.empty()) {
        LogWarning("Store name is empty", "validation");
        return VALIDATION_WARNING;
    }
    
    return VALIDATION_SUCCESS;
}

DataPersistenceManager::ValidationResult DataPersistenceManager::ValidateArchives()
{
    FnTrace("DataPersistenceManager::ValidateArchives()");
    
    if (!system_ref) {
        LogError("Cannot validate archives - system reference is null", "validation");
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
        LogError("Cannot validate terminals - MasterControl is null", "validation");
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
        LogError("Cannot save checks - system reference is null", "save");
        return SAVE_FAILED;
    }

    int saved_count = 0;
    int total_count = 0;
    int failed_count = 0;

    // Single pass through the linked list for efficiency
    // Add safety limit to prevent infinite loops from corrupted linked lists
    const int MAX_CHECKS = 100000;  // Reasonable upper limit
    Check* check = system_ref->CheckList();
    while (check != nullptr && total_count < MAX_CHECKS) {
        total_count++;

        // Skip training checks - we don't want to backup/save training data
        if (check->IsTraining()) {
            LogInfo("Skipping training check (serial: " + std::to_string(check->serial_number) + ")");
            check = check->next;
            continue;
        }

        // Skip invalid checks that can't be saved
        if (check->serial_number <= 0) {
            failed_count++;
            if (failed_count <= 5) {
                LogWarning("Skipping save of check with invalid serial number", "save");
            }
            check = check->next;
            continue;
        }

        if (check->Save() == 0) {
            saved_count++;
        } else {
            failed_count++;
            // Only log first few failures to avoid log spam
            if (failed_count <= 5) {
                LogError("Failed to save check with serial number: " + std::to_string(check->serial_number), "save");
            }
        }

        check = check->next;
    }

    // Check if we hit the iteration limit (possible corrupted linked list)
    if (total_count >= MAX_CHECKS) {
        LogError("SaveAllChecks() hit iteration limit (" + std::to_string(MAX_CHECKS) + 
                 "), possible infinite loop prevented. Check list may be corrupted.", "save");
    }

    // Handle empty check list
    if (total_count == 0) {
        return SAVE_SUCCESS;
    }

    // Log summary if there were many failures
    if (failed_count > 5) {
        LogError("Failed to save " + std::to_string(failed_count) + " checks out of " +
                std::to_string(total_count) + " total", "save");
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
        LogError("Cannot save settings - system reference is null", "save");
        return SAVE_FAILED;
    }

    Settings* settings = &system_ref->settings;
    if (!settings) {
        LogError("Settings object is null", "save");
        return SAVE_FAILED;
    }

    int save_result = settings->Save();
    if (save_result == 0) {
        return SAVE_SUCCESS;
    } else {
        LogError("Failed to save settings (error code: " + std::to_string(save_result) + ")", "save");
        return SAVE_FAILED;
    }
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllArchives()
{
    FnTrace("DataPersistenceManager::SaveAllArchives()");
    
    if (!system_ref) {
        LogError("Cannot save archives - system reference is null", "save");
        return SAVE_FAILED;
    }
    
    return system_ref->SaveChanged() == 0 ? SAVE_SUCCESS : SAVE_FAILED;
}

DataPersistenceManager::SaveResult DataPersistenceManager::SaveAllTerminals()
{
    FnTrace("DataPersistenceManager::SaveAllTerminals()");
    
    if (!MasterControl) {
        LogError("Cannot save terminals - MasterControl is null", "save");
        return SAVE_FAILED;
    }
    
    // Don't force exit from edit mode during periodic saves
    // Only save terminal data if not in edit mode to avoid interrupting user workflow
    Terminal* term = MasterControl->TermList();
    while (term != nullptr) {
        // Only save if not in edit mode - let users complete their edits
        if (term->edit == 0) {
            // Terminal is not in edit mode, safe to save any pending changes
            // This would be where we'd save terminal-specific data if needed
        }
        // If term->edit > 0, skip saving to avoid interrupting edit mode
        term = term->next;
    }
    
    return SAVE_SUCCESS;
}

// CUPS monitoring methods
bool DataPersistenceManager::CheckCUPSHealth()
{
    FnTrace("DataPersistenceManager::CheckCUPSHealth()");
    
    // During shutdown, skip CUPS health checks to avoid hanging
    if (shutdown_in_progress.load(std::memory_order_acquire)) {
        LogInfo("Skipping CUPS health check during shutdown to prevent hanging");
        return true; // Assume healthy to avoid blocking shutdown
    }
    
    // Use safe command execution with timeout
    // Check if CUPS daemon is running
    int result = ExecuteCommandWithTimeout("systemctl is-active --quiet cups", config.system_call_timeout);
    if (result != 0) {
        LogWarning("CUPS daemon is not running or check timed out", "cups");
        cups_consecutive_failures++;
        return false;
    }

    // Check if we can communicate with CUPS
    result = ExecuteCommandWithTimeout("lpstat -r > /dev/null 2>&1", config.system_call_timeout);
    if (result != 0) {
        LogWarning("Cannot communicate with CUPS (lpstat failed or timed out)", "cups");
        cups_consecutive_failures++;
        return false;
    }

    // Reset consecutive failures on success
    cups_consecutive_failures = 0;
    
    return true;
}

void DataPersistenceManager::AttemptCUPSRecovery()
{
    FnTrace("DataPersistenceManager::AttemptCUPSRecovery()");
    
    // Skip CUPS recovery during shutdown to avoid hanging
    if (shutdown_in_progress) {
        LogInfo("Skipping CUPS recovery during shutdown to prevent hanging");
        return;
    }
    
    LogInfo("Attempting CUPS recovery");

    // Try to restart CUPS service with timeout
    int result = ExecuteCommandWithTimeout("systemctl restart cups", config.system_call_timeout * 2);
    if (result == 0) {
        LogInfo("CUPS service restarted successfully");

        // Wait a moment for service to start
        sleep(2);

        // Check if recovery was successful
        if (CheckCUPSHealth()) {
            cups_communication_healthy = true;
            cups_consecutive_failures = 0;
            LogInfo("CUPS recovery successful");
        } else {
            LogError("CUPS recovery failed - service restarted but still unhealthy", "cups");
        }
    } else {
        LogError("Failed to restart CUPS service or operation timed out", "cups");
    }
}

// Error recovery methods
bool DataPersistenceManager::AttemptRecovery(const std::string& component)
{
    FnTrace("DataPersistenceManager::AttemptRecovery()");

    if (component == "cups") {
        if (cups_consecutive_failures >= 3) {
            LogWarning("Attempting recovery for CUPS after " + std::to_string(cups_consecutive_failures) + " consecutive failures", "recovery");
            ForceCUPSRecovery();
            return true;
        }
    } else if (component == "checks" || component == "settings" || component == "archives") {
        // For data components, try emergency save
        LogWarning("Attempting recovery for " + component + " component", "recovery");
        EmergencySave();
        return true;
    }

    return false;
}

void DataPersistenceManager::ResetFailureCounters()
{
    cups_consecutive_failures = 0;
    for (auto& data_item : critical_data_items) {
        data_item.consecutive_failures = 0;
    }
    LogInfo("Failure counters reset");
}

// Detailed save operations with metrics
DataPersistenceManager::OperationResult DataPersistenceManager::SaveAllDataDetailed()
{
    FnTrace("DataPersistenceManager::SaveAllDataDetailed()");

    auto start_time = std::chrono::steady_clock::now();
    int total_processed = 0;
    int total_failed = 0;

    // Save all registered data
    for (const auto& callback : save_callbacks) {
        SaveResult result = callback();
        if (result != SAVE_SUCCESS) {
            total_failed++;
        }
        total_processed++;
    }

    for (const auto& data_item : critical_data_items) {
        SaveResult result = data_item.saver();
        if (result != SAVE_SUCCESS) {
            total_failed++;
        }
        total_processed++;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Update metrics
    metrics.total_saves++;
    if (total_failed > 0) {
        metrics.failed_saves++;
    }

    SaveResult overall_result = (total_failed == 0) ? SAVE_SUCCESS :
                               (total_failed < total_processed * 0.2) ? SAVE_PARTIAL : SAVE_FAILED;

    return OperationResult(overall_result, "Saved " + std::to_string(total_processed - total_failed) +
                          "/" + std::to_string(total_processed) + " components", total_processed, total_failed, duration);
}

DataPersistenceManager::OperationResult DataPersistenceManager::SaveCriticalDataDetailed()
{
    FnTrace("DataPersistenceManager::SaveCriticalDataDetailed()");

    auto start_time = std::chrono::steady_clock::now();
    int total_processed = 0;
    int total_failed = 0;

    for (auto& data_item : critical_data_items) {
        SaveResult result = data_item.saver();
        if (result != SAVE_SUCCESS) {
            total_failed++;
            data_item.consecutive_failures++;
            data_item.last_failure = std::chrono::steady_clock::now();
        } else {
            data_item.consecutive_failures = 0; // Reset on success
        }
        total_processed++;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Update metrics
    metrics.total_saves++;
    if (total_failed > 0) {
        metrics.failed_saves++;
    }

    SaveResult overall_result = (total_failed == 0) ? SAVE_SUCCESS :
                               (total_failed < total_processed * 0.2) ? SAVE_PARTIAL : SAVE_FAILED;

    return OperationResult(overall_result, "Saved " + std::to_string(total_processed - total_failed) +
                          "/" + std::to_string(total_processed) + " critical components", total_processed, total_failed, duration);
}

// Performance monitoring methods
std::string DataPersistenceManager::GeneratePerformanceReport() const
{
    std::ostringstream report;
    report << "=== Performance Report ===\n";
    report << "Total validations: " << metrics.total_validations << "\n";
    report << "Failed validations: " << metrics.failed_validations << "\n";
    report << "Total saves: " << metrics.total_saves << "\n";
    report << "Failed saves: " << metrics.failed_saves << "\n";
    report << "Total validation time: " << metrics.total_validation_time.count() << "ms\n";
    report << "Total save time: " << metrics.total_save_time.count() << "ms\n";
    report << "Average validation time: " <<
        (metrics.total_validations > 0 ? metrics.total_validation_time.count() / metrics.total_validations : 0) << "ms\n";
    report << "Average save time: " <<
        (metrics.total_saves > 0 ? metrics.total_save_time.count() / metrics.total_saves : 0) << "ms\n";
    report << "Save success rate: " << GetSaveSuccessRate() * 100 << "%\n";
    report << "Validation success rate: " << GetValidationSuccessRate() * 100 << "%\n";

    return report.str();
}

void DataPersistenceManager::ResetPerformanceMetrics()
{
    metrics.total_validations = 0;
    metrics.total_saves = 0;
    metrics.failed_validations = 0;
    metrics.failed_saves = 0;
    metrics.total_validation_time = std::chrono::milliseconds(0);
    metrics.total_save_time = std::chrono::milliseconds(0);
    metrics.last_reset = std::chrono::steady_clock::now();
    LogInfo("Performance metrics reset");
}

double DataPersistenceManager::GetSaveSuccessRate() const
{
    if (metrics.total_saves == 0) return 1.0;
    return 1.0 - (static_cast<double>(metrics.failed_saves) / metrics.total_saves);
}

double DataPersistenceManager::GetValidationSuccessRate() const
{
    if (metrics.total_validations == 0) return 1.0;
    return 1.0 - (static_cast<double>(metrics.failed_validations) / metrics.total_validations);
}

// Logging methods
void DataPersistenceManager::LogError(const std::string& message)
{
    LogError(message, "general");
}

void DataPersistenceManager::LogError(const std::string& message, const std::string& component)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    const auto max_error_size = static_cast<std::size_t>(config.max_error_log_size);
    if (error_log.size() >= max_error_size) {
        // Remove oldest entries to make room
        error_log.erase(error_log.begin(),
                        error_log.begin() + static_cast<std::ptrdiff_t>(
                            error_log.size() - max_error_size + 1));
    }
    error_log.emplace_back(message, component, 2);
    ReportError(message.c_str());
}

void DataPersistenceManager::LogWarning(const std::string& message)
{
    LogWarning(message, "general");
}

void DataPersistenceManager::LogWarning(const std::string& message, const std::string& component)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    const auto max_warning_size = static_cast<std::size_t>(config.max_warning_log_size);
    if (warning_log.size() >= max_warning_size) {
        // Remove oldest entries to make room
        warning_log.erase(warning_log.begin(),
                          warning_log.begin() + static_cast<std::ptrdiff_t>(
                              warning_log.size() - max_warning_size + 1));
    }
    warning_log.emplace_back(message, component, 1);
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
