# ViewTouch Data Persistence System

## Overview

The ViewTouch Data Persistence System is a comprehensive solution designed to address data loss issues that occur when the system runs for extended periods without restart, particularly when CUPS communication fails and checks are not properly saved during shutdown/restart cycles.

## Problem Statement

The original issue involved:
1. System stops communicating with CUPS after long periods of operation
2. After restart, some checks are not saved and are deleted
3. No comprehensive validation that all data is saved before shutdown/restart

## Solution Architecture

### DataPersistenceManager Class

The core of the solution is the `DataPersistenceManager` singleton class that provides:

#### Key Features
- **Comprehensive Data Validation**: Validates all critical data before shutdown
- **Automatic Periodic Saving**: Auto-saves critical data at configurable intervals
- **CUPS Communication Monitoring**: Monitors and recovers from CUPS communication failures
- **Data Integrity Tracking**: Tracks which data is "dirty" and needs saving
- **Emergency Save Procedures**: Handles critical failures with emergency data saving
- **Comprehensive Logging**: Detailed logging of all save operations and errors

#### Data Types Monitored
1. **Checks**: All open and closed checks
2. **Settings**: System configuration and settings
3. **Archives**: Historical data archives
4. **Terminals**: Terminal state and configuration
5. **CUPS Communication**: Printer communication health

### Integration Points

#### System Initialization
```cpp
// In manager.cc - StartSystem()
MasterSystem = std::make_unique<System>();
InitializeDataPersistence(MasterSystem.get());
```

#### Periodic Updates
```cpp
// In manager.cc - UpdateSystemCB()
GetDataPersistenceManager().Update();
```

#### Shutdown Process
```cpp
// In manager.cc - EndSystem()
DataPersistenceManager::SaveResult save_result = GetDataPersistenceManager().PrepareForShutdown();
```

#### Check Saving Integration
```cpp
// In check.cc - Check::Save()
GetDataPersistenceManager().MarkDataDirty("checks");
// ... save logic ...
GetDataPersistenceManager().MarkDataClean("checks");
```

## Configuration

### Auto-Save Settings
- **Default Interval**: 30 seconds
- **Configurable**: Can be adjusted via `SetAutoSaveInterval()`
- **Enable/Disable**: Can be toggled via `EnableAutoSave()`

### CUPS Monitoring
- **Check Interval**: 60 seconds (configurable)
- **Health Validation**: Checks CUPS daemon status and communication
- **Automatic Recovery**: Attempts to restart CUPS service on failure

### Data Validation
- **Pre-Shutdown Validation**: Validates all critical data before shutdown
- **Integrity Checks**: Verifies data consistency and completeness
- **Error Reporting**: Detailed error logging and reporting

## Error Handling

### Validation Results
- `VALIDATION_SUCCESS`: All data is valid
- `VALIDATION_WARNING`: Minor issues detected
- `VALIDATION_ERROR`: Significant issues detected
- `VALIDATION_CRITICAL`: Critical issues requiring immediate attention

### Save Results
- `SAVE_SUCCESS`: All data saved successfully
- `SAVE_PARTIAL`: Some data saved, some failed
- `SAVE_FAILED`: Save operation failed
- `SAVE_CRITICAL_FAILURE`: Critical save failure

### Emergency Procedures
- **Emergency Save**: Saves only the most critical data
- **Backup Creation**: Creates timestamped backups before shutdown
- **Recovery**: Provides backup restoration capabilities

## Monitoring and Diagnostics

### Logging System
- **Error Log**: Tracks all errors and failures
- **Warning Log**: Tracks warnings and potential issues
- **Info Log**: Tracks normal operations and status changes

### Integrity Reports
- **GenerateIntegrityReport()**: Creates comprehensive system status report
- **HasDataIntegrityIssues()**: Quick check for data integrity problems
- **GetErrorLog()**: Retrieves error history
- **GetWarningLog()**: Retrieves warning history

## Usage Examples

### Basic Usage
```cpp
// Get the persistence manager instance
DataPersistenceManager& manager = GetDataPersistenceManager();

// Check if system can safely shutdown
if (manager.CanSafelyShutdown()) {
    // Safe to shutdown
} else {
    // Data needs to be saved first
    manager.SaveAllData();
}

// Generate integrity report
std::string report = manager.GenerateIntegrityReport();
```

### Advanced Configuration
```cpp
// Configure auto-save interval
manager.SetAutoSaveInterval(std::chrono::seconds(60));

// Enable/disable auto-save
manager.EnableAutoSave(true);

// Configure CUPS check interval
manager.SetCUPSCheckInterval(std::chrono::seconds(30));

// Force CUPS recovery
manager.ForceCUPSRecovery();
```

### Emergency Procedures
```cpp
// Emergency save of critical data
manager.EmergencySave();

// Create backup
manager.CreateBackup();

// Restore from backup
manager.RestoreFromBackup("/path/to/backup");
```

## Benefits

1. **Data Loss Prevention**: Comprehensive validation and saving prevents data loss
2. **CUPS Communication Recovery**: Automatic detection and recovery from CUPS failures
3. **Proactive Monitoring**: Continuous monitoring of data integrity
4. **Detailed Logging**: Complete audit trail of all operations
5. **Emergency Procedures**: Robust handling of critical failures
6. **Configurable**: Flexible configuration for different environments
7. **Non-Intrusive**: Minimal impact on existing system performance

## Implementation Details

### Thread Safety
- Uses mutex locks for thread-safe operations
- Singleton pattern with proper initialization
- Safe concurrent access to shared data

### Memory Management
- Uses smart pointers for automatic memory management
- Proper cleanup in destructor
- No memory leaks or dangling pointers

### Error Recovery
- Graceful degradation on errors
- Automatic retry mechanisms
- Fallback procedures for critical failures

## Testing and Verification

The system includes comprehensive logging and monitoring that allows for:
- Verification that all data is saved before shutdown
- Detection of CUPS communication issues
- Monitoring of data integrity over time
- Validation of backup and recovery procedures

## Future Enhancements

Potential future improvements include:
- Database-level transaction support
- Real-time data replication
- Advanced backup strategies
- Performance optimization
- Integration with external monitoring systems

## Conclusion

The Data Persistence System provides a robust solution to the data loss issues in ViewTouch, ensuring that all critical data is properly validated and saved before system shutdown/restart, while providing comprehensive monitoring and recovery capabilities for CUPS communication failures.
