#ifndef DATA_PERSISTENCE_MANAGER_HH
#define DATA_PERSISTENCE_MANAGER_HH

#include "basic.hh"
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <string>
#include <atomic>

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
 *
 * Features:
 * - Singleton pattern for global access
 * - Configurable auto-save intervals
 * - Critical data validation and saving
 * - CUPS communication monitoring and recovery
 * - Comprehensive error handling and recovery
 * - Thread-safe operations
 * - Data integrity monitoring
 * - Backup and restore capabilities
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

    // Configuration structure
    struct Configuration {
        std::chrono::seconds auto_save_interval = std::chrono::seconds(30);
        std::chrono::seconds cups_check_interval = std::chrono::seconds(60);
        std::chrono::seconds system_call_timeout = std::chrono::seconds(5);
        int max_error_log_size = 1000;
        int max_warning_log_size = 1000;
        bool enable_cups_monitoring = true;
        bool enable_auto_save = true;
        bool enable_backup_on_shutdown = false;
        std::string backup_directory = "/tmp/viewtouch_backups";
    };

    // Error information structure
    struct ErrorInfo {
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string component;
        int severity; // 0=info, 1=warning, 2=error, 3=critical

                ErrorInfo(std::string msg, std::string comp, int sev = 2)
                        : message(std::move(msg)), timestamp(std::chrono::system_clock::now()),
                            component(std::move(comp)), severity(sev) {}
    };

    // Data operation result with detailed information
    struct OperationResult {
        SaveResult result;
        std::string details;
        int items_processed;
        int items_failed;
        std::chrono::milliseconds duration;

        OperationResult(SaveResult res = SAVE_SUCCESS, std::string det = "",
                       int processed = 0, int failed = 0,
                       std::chrono::milliseconds dur = std::chrono::milliseconds(0))
            : result(res), details(std::move(det)), items_processed(processed),
              items_failed(failed), duration(dur) {}
    };

private:
    // Singleton instance
    static std::unique_ptr<DataPersistenceManager> instance;
    static std::mutex instance_mutex;

    // System reference
    System* system_ref;

    // Configuration
    Configuration config;

    // Auto-save state
    std::chrono::steady_clock::time_point last_auto_save;

    // Data validation callbacks
    std::vector<ValidationCallback> validation_callbacks;

    // Data save callbacks
    std::vector<SaveCallback> save_callbacks;

    // Critical data tracking
    struct CriticalData {
        std::string name;
        bool is_dirty{false};
        std::chrono::steady_clock::time_point last_modified;
        ValidationCallback validator;
        SaveCallback saver;
        int consecutive_failures{0};
        std::chrono::steady_clock::time_point last_failure;

        CriticalData() = default;
    };
    std::vector<CriticalData> critical_data_items;

    // CUPS communication monitoring
    std::atomic<bool> cups_communication_healthy;
    std::chrono::steady_clock::time_point last_cups_check;
    std::atomic<int> cups_consecutive_failures;

    // Enhanced logging and error tracking
    std::vector<ErrorInfo> error_log;
    std::vector<ErrorInfo> warning_log;
    mutable std::mutex log_mutex;

    // Configuration access mutex
    mutable std::mutex config_mutex;

    // Shutdown state - atomic for thread safety
    std::atomic<bool> shutdown_in_progress;
    std::atomic<bool> force_shutdown;

    // Performance metrics
    struct PerformanceMetrics {
        int total_validations{0};
        int total_saves{0};
        int failed_validations{0};
        int failed_saves{0};
        std::chrono::milliseconds total_validation_time{0};
        std::chrono::milliseconds total_save_time{0};
        std::chrono::steady_clock::time_point last_reset;

        PerformanceMetrics()
            : last_reset(std::chrono::steady_clock::now()) {}
    };
    PerformanceMetrics metrics;
    
    
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
    void LogError(const std::string& message, const std::string& component);
    void LogWarning(const std::string& message);
    void LogWarning(const std::string& message, const std::string& component);
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
    void SetConfiguration(const Configuration& new_config);
    const Configuration& GetConfiguration() const;
    
    // Data validation
    ValidationResult ValidateAllData();
    ValidationResult ValidateCriticalData();
    void RegisterValidationCallback(const std::string& name, const ValidationCallback& callback);
    
    // Data saving
    SaveResult SaveAllData();
    SaveResult SaveCriticalData();
    OperationResult SaveAllDataDetailed();
    OperationResult SaveCriticalDataDetailed();
    void RegisterSaveCallback(const std::string& name, const SaveCallback& callback);
    
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
    std::vector<ErrorInfo> GetDetailedErrorLog() const;
    std::vector<ErrorInfo> GetDetailedWarningLog() const;
    void ClearLogs();
    bool IsAnyTerminalInEditMode() const;

    // Error recovery
    bool AttemptRecovery(const std::string& component);
    void ResetFailureCounters();
    
    // Data integrity reports
    std::string GenerateIntegrityReport() const;
    bool HasDataIntegrityIssues() const;

    // Data integrity verification
    bool VerifyFileIntegrity(const std::string& file_path) const;
    bool VerifyDataConsistency() const;
    std::string GenerateDataChecksum(const std::string& data_type) const;
    
    // Emergency procedures
    void EmergencySave();
    void CreateBackup();
    bool RestoreFromBackup(const std::string& backup_path);

    // Performance monitoring
    std::string GeneratePerformanceReport() const;
    void ResetPerformanceMetrics();
    double GetSaveSuccessRate() const;
    double GetValidationSuccessRate() const;
};

// Global convenience functions
DataPersistenceManager& GetDataPersistenceManager();
void InitializeDataPersistence(System* system);
void ShutdownDataPersistence();

#endif // DATA_PERSISTENCE_MANAGER_HH
