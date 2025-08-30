# F3/F4 Recording Control Feature

## Overview

This feature adds user control over the F3/F4 recording and replay functionality in ViewTouch, allowing administrators to enable or disable this debugging feature based on their needs.

## Background

The F3/F4 recording feature is a powerful debugging tool that allows developers and testers to:
- **F3**: Start/stop recording a sequence of touches and keystrokes
- **F4**: Replay the recorded sequence
- **Double F3**: Erase the recorded sequence

However, this feature was previously only controlled by the `debug_mode` flag and could cause issues in production environments where users might accidentally trigger long replay sequences.

## New Implementation

### Settings Integration

The F3/F4 recording feature is now controlled through the ViewTouch Soft Switches page:

- **Setting Name**: `enable_f3_f4_recording`
- **Location**: Soft Switches page → "Enable F3/F4 Recording/Replay?" toggle
- **Default Value**: Disabled (0) for safety
- **UI Control**: On/Off toggle button that shows current status

### Access Control

The feature now requires only **one condition** to be met:
1. `enable_f3_f4_recording` setting must be enabled (Soft Switch)

This provides direct user control:
- **Single Control Point**: Users can enable/disable recording directly from the Soft Switches page
- **No Debug Mode Required**: Eliminates the need for system-level debug configuration
- **Cleaner Interface**: Simpler user experience with fewer configuration steps

## Usage

### For Developers and Testers

1. **Enable Recording**: Go to Soft Switches page → "Enable F3/F4 Recording/Replay?" → On
2. **Use Recording**:
   - Press **F3** to start recording touches and keystrokes
   - Perform the actions you want to record
   - Press **F3** again to stop recording
   - Press **F4** to replay the recorded sequence
   - Press **F3** twice quickly to erase the recording

### For Production Environments

- **Default**: Recording is disabled by default for safety
- **Selective Enable**: Can be enabled only for specific terminals or users who need it
- **Easy Disable**: Can be quickly disabled if issues arise

## Benefits

### Safety Improvements
- **Accidental Prevention**: Users can't accidentally trigger recording/replay
- **Production Safe**: Default disabled state prevents production issues
- **Controlled Access**: Only authorized users can enable the feature

### Developer Experience
- **Easy Testing**: Quick enable/disable for development work
- **Consistent Control**: Same settings interface as other features
- **Clear Documentation**: Obvious setting name and purpose

### System Stability
- **Reduced Risk**: Eliminates accidental long replay sequences
- **User Control**: Administrators can manage feature access
- **Backward Compatible**: Existing functionality preserved when enabled

## Technical Details

### Code Changes

1. **Settings Class**: Added `enable_f3_f4_recording` variable
2. **Soft Switches UI**: Added On/Off toggle button in SwitchZone
3. **Terminal Logic**: Modified F3/F4 handlers to check the new setting
4. **Default Values**: Set to disabled (0) for safety

### File Modifications

- `main/settings.hh`: Added new setting variable
- `main/settings.cc`: Added default value in constructor
- `main/labels.cc`: Added switch name and value to arrays
- `zone/settings_zone.cc`: Added switch handling in SwitchZone
- `main/terminal.cc`: Modified F3/F4 key handlers

### Settings Persistence

The setting is automatically saved with other settings and persists across system restarts.

## Configuration Examples

### Enable for Development
```cpp
// In settings or configuration
enable_f3_f4_recording = 1;
```

### Disable for Production
```cpp
// Default safe configuration
enable_f3_f4_recording = 0;
```

### Conditional Enable
```cpp
// Enable only for specific terminals
if (terminal_id == DEV_TERMINAL_ID) {
    enable_f3_f4_recording = 1;
}
```

## Future Enhancements

### Potential Improvements
- **User-Level Control**: Allow specific users to enable/disable recording
- **Recording Limits**: Add maximum recording duration or size limits
- **Recording History**: Store multiple recordings with names/descriptions
- **Export/Import**: Allow sharing recordings between systems
- **Recording Validation**: Check recording validity before replay

### Integration Opportunities
- **Training Mode**: Use recordings for user training scenarios
- **Automated Testing**: Integrate with testing frameworks
- **Quality Assurance**: Standardize testing procedures with recordings
- **Documentation**: Create visual guides using recorded sequences

## Conclusion

The F3/F4 recording control feature provides a safe, user-controlled way to access powerful debugging tools while maintaining system stability in production environments. By integrating with the existing settings system and providing clear controls, it enhances both developer productivity and system reliability.
