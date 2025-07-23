# Embossed Text Feature Implementation

## Overview

This implementation adds an optional embossed text effect to the ViewTouch system, providing a 'frosted' effect on the top and left edges and a 'shadow' effect on the bottom and right edges of text characters. This enhances text contrast and readability while maintaining the option to disable it for performance.

## Features

- **User Configurable**: The embossed text effect can be enabled or disabled through the settings interface
- **Enhanced Readability**: Creates visual depth with highlight and shadow effects
- **Performance Conscious**: Can be disabled to reduce rendering overhead
- **Backward Compatible**: Defaults to enabled but can be turned off

## Implementation Details

### 1. Settings Integration

The feature is integrated into the existing settings system:

- **Setting Name**: `use_embossed_text`
- **Location**: Settings page (General Settings section)
- **Default Value**: Enabled (1) for better readability
- **Settings Version**: 96 (new version for this feature)

### 2. Text Rendering Pipeline

The embossed text effect is implemented in the text rendering pipeline:

1. **Layer::Text()** method now accepts an additional `embossed` parameter
2. **GenericDrawStringXftEmbossed()** function provides the embossed rendering
3. **Terminal view** receives the setting via `TERM_SET_EMBOSSED` command
4. **Settings UI** allows users to toggle the feature on/off

### 3. Embossed Effect Algorithm

The embossed effect creates three layers of text:

1. **Shadow Layer**: Much darker version of the text color (33% intensity) offset by +2,+2 pixels
2. **Highlight Layer**: Lighter version created by adding 40% of remaining white to the original color, offset by -2,-2 pixels  
3. **Main Layer**: Original text color at the base position

This creates a balanced embossed effect with proper lighting and enhanced contrast. The highlight calculation ensures visible lightening while maintaining readability.

### 4. Files Modified

#### Core Implementation
- `generic_char.hh` - Added function declaration
- `generic_char.cc` - Implemented embossed text rendering
- `term/layer.hh` - Updated Text method signature
- `term/layer.cc` - Modified text rendering to use embossed effect

#### Settings System
- `main/settings.hh` - Added use_embossed_text setting
- `main/settings.cc` - Added load/save logic for the setting
- `zone/settings_zone.cc` - Added UI control for the setting

#### Terminal Communication
- `remote_link.hh` - Added TERM_SET_EMBOSSED command
- `term/term_view.hh` - Added global variable declaration
- `term/term_view.cc` - Added command handler and text rendering integration
- `main/terminal.hh` - Added SetEmbossedText method
- `main/terminal.cc` - Implemented SetEmbossedText and initialization
- `debug.cc` - Added debug support for new command

## Usage

### For Users

1. Navigate to Settings in the ViewTouch interface
2. Find "Use Embossed Text Effects?" in the Miscellaneous Settings section
3. Toggle between "Yes" (enabled) and "No" (disabled)
4. Save settings to apply changes

### For Developers

The embossed text effect is automatically applied to all text rendering when enabled. No code changes are required in individual zones or components.

## Performance Considerations

- **Enabled**: Slightly slower rendering due to drawing three text layers
- **Disabled**: Standard rendering performance
- **Memory**: Minimal additional memory usage for color calculations

## Testing

The embossed text feature can be tested by:

1. Building and running the ViewTouch system
2. Navigating to Settings and toggling the "Use Embossed Text Effects?" option
3. Observing the visual difference in text rendering throughout the interface

## Future Enhancements

Potential improvements could include:

1. **Intensity Control**: Allow users to adjust the embossed effect intensity
2. **Direction Control**: Allow different embossed directions (top-left, bottom-right, etc.)
3. **Color Customization**: Allow custom highlight and shadow colors
4. **Selective Application**: Apply embossed effects only to specific UI elements

## Technical Notes

- The feature uses Xft for high-quality font rendering
- Color calculations are done in 16-bit color space (0-65535)
- The effect works with all existing fonts and colors
- No changes to the existing text positioning or layout systems 