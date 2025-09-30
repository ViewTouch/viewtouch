#ifndef DATA_PERSISTENCE_MANAGER_HH
#define DATA_PERSISTENCE_MANAGER_HH

#include "basic.hh"
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>

// Forward declarations
class System;
class Check;
class Settings;
class Archive;
class Terminal;

/**
 * DataPersistenceManager - Comprehensive data persistence and validation system
 * 
 * This class ensures that all critical data is properly saved before system
 * shutdown/restart and provides monitoring for data integrity issues.
 */
class DataPersistenceManager
{
public:
    // Data validation result codes
    enum ValidationResult {
        VALIDATION_SUCCESS = 0,
        VALIDATION_WARNING = 1,
        VALIDATION_ERROR = 2,
        VALIDATION_CRITICAL = 3
    };

    // Save operation result codes
    enum SaveResult {
        SAVE_SUCCESS = 0,
        SAVE_PARTIAL = 1,
        SAVE_FAILED = 2,
        SAVE_CRITICAL_FAILURE = 3
    };

    // Data validation callback function type
    using ValidationCallback = std::function<ValidationResult()>;
    
    // Data save callback function type
    using SaveCallback = std::function<SaveResult()>;

private:
    // Singleton instance
    static std::unique_ptr<DataPersistenceManager> instance;
    static std::mutex instance_mutex;

    // System reference
    System* system_ref;
    
    // Auto-save configuration
    std::chrono::seconds auto_save_interval;
    std::chrono::steady_clock::time_point last_auto_save;
    bool auto_save_enabled;
    
    // Data validation callbacks
    std::vector<ValidationCallback> validation_callbacks;
    
    // Data save callbacks
    std::vector<SaveCallback> save_callbacks;
    
    // Critical data tracking
    struct CriticalData {
        std::string name;
        bool is_dirty;
        std::chrono::steady_clock::time_point last_modified;
        ValidationCallback validator;
        SaveCallback saver;
    };
    std::vector<CriticalData> critical_data_items;
    
    // CUPS communication monitoring
    bool cups_communication_healthy;
    std::chrono::steady_clock::time_point last_cups_check;
    std::chrono::seconds cups_check_interval;
    
    // Logging and error tracking
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    mutable std::mutex log_mutex;
    
    // Shutdown state tracking
    bool shutdown_in_progress;
    bool force_shutdown;
    
    // Constructor (private for singleton)
    DataPersistenceManager();
    
    // Internal validation methods
    ValidationResult ValidateChecks();
    ValidationResult ValidateSettings();
    ValidationResult ValidateArchives();
    ValidationResult ValidateTerminals();
    ValidationResult ValidateCUPSCommunication();
    
    // Internal save methods
    SaveResult SaveAllChecks();
    SaveResult SaveAllSettings();
    SaveResult SaveAllArchives();
    SaveResult SaveAllTerminals();
    
    // CUPS monitoring methods
    bool CheckCUPSHealth();
    void AttemptCUPSRecovery();
    
    // Logging methods
    void LogError(const std::string& message);
    void LogWarning(const std::string& message);
    void LogInfo(const std::string& message);

public:
    // Singleton access
    static DataPersistenceManager& GetInstance();
    static void Initialize(System* system);
    static void Shutdown();
    
    // Destructor
    ~DataPersistenceManager();
    
    // Configuration
    void SetAutoSaveInterval(std::chrono::seconds interval);
    void EnableAutoSave(bool enable);
    void SetCUPSCheckInterval(std::chrono::seconds interval);
    
    // Data validation
    ValidationResult ValidateAllData();
    ValidationResult ValidateCriticalData();
    void RegisterValidationCallback(const std::string& name, ValidationCallback callback);
    
    // Data saving
    SaveResult SaveAllData();
    SaveResult SaveCriticalData();
    void RegisterSaveCallback(const std::string& name, SaveCallback callback);
    
    // Critical data management
    void RegisterCriticalData(const std::string& name, 
                             ValidationCallback validator, 
                             SaveCallback saver);
    void MarkDataDirty(const std::string& name);
    void MarkDataClean(const std::string& name);
    bool IsDataDirty(const std::string& name) const;
    
    // CUPS monitoring
    bool IsCUPSHealthy() const;
    void CheckCUPSStatus();
    void ForceCUPSRecovery();
    
    // Periodic operations
    void ProcessPeriodicTasks();
    void Update();
    
    // Shutdown procedures
    SaveResult PrepareForShutdown();
    SaveResult ForceShutdown();
    bool CanSafelyShutdown() const;
    
    // Status and diagnostics
    std::vector<std::string> GetErrorLog() const;
    std::vector<std::string> GetWarningLog() const;
    void ClearLogs();
    bool IsAnyTerminalInEditMode() const;
    
    // Data integrity reports
    std::string GenerateIntegrityReport() const;
    bool HasDataIntegrityIssues() const;
    
    // Emergency procedures
    void EmergencySave();
    void CreateBackup();
    bool RestoreFromBackup(const std::string& backup_path);
};

// Global convenience functions
DataPersistenceManager& GetDataPersistenceManager();
void InitializeDataPersistence(System* system);
void ShutdownDataPersistence();

#endif // DATA_PERSISTENCE_MANAGER_HH
