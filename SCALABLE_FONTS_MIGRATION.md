# ViewTouch Scalable Fonts & Buffer Safety Migration

## Overview

This document describes the modernization of ViewTouch, focusing on two major areas:

1. Migration from bitmapped fonts to scalable outline fonts using Xft (X FreeType interface).
2. Comprehensive buffer safety improvements by replacing unsafe string functions (`sprintf`, `strcat`) with their safe counterparts (`snprintf`, `strncat`).

These changes provide high-quality, scalable text rendering and robust protection against buffer overflows, making the codebase more secure and maintainable.

---

## 1. Scalable Fonts Migration

### 1.1. **Updated Core Text Rendering (`term/layer.cc`)**
- **Added Xft Include**: `#include <X11/Xft/Xft.h>`
- **Updated `Layer::Text()` Function**:
  - Changed from `XFontStruct *` to `XftFont *`
  - Replaced `XTextWidth()` with `XftTextExtentsUtf8()`
  - Used `font_info->ascent` instead of `GetFontBaseline()`
  - Replaced `GenericDrawString()` with `XftDrawStringUtf8()`
  - **Preserved Embossed Text Effect**: Three-pass rendering (shadow, highlight, main text)
  - Added proper XftColor allocation and cleanup
- **Updated `Layer::ZoneText()` Function**:
  - Changed to use `XftFont *` and `XftTextExtentsUtf8()`
  - Used `font_info->height` directly

### 1.2. **Updated Font System (`term/term_view.cc`)**
- **Added Scalable Font Names Function**:
  ```cpp
  const char* GetScalableFontName(int font_id)
  ```
  Maps font IDs to modern scalable font specifications like:
  - `"Times:size=24:style=regular"`
  - `"Times:size=34:style=bold"`
  - `"Courier:size=18:style=regular"`
- **Updated Font Arrays**:
  - Changed `XFontStruct *FontInfo[32]` to `XftFont *FontInfo[32]`
  - Updated font loading to use `XftFontOpenName()`
  - Added fallback to default font if scalable font fails
- **Updated Font Functions**:
  - `GetFontInfo()` now returns `XftFont *`
  - Font cleanup uses `XftFontClose()` instead of `XFreeFont()`

### 1.3. **Updated Header Files (`term/term_view.hh`)**
- Added `#include <X11/Xft/Xft.h>`
- Updated `GetFontInfo()` function declaration to return `XftFont *`

### 1.4. **Updated Manager Functions (`main/manager.cc`)**
- **Added Xft Include**: `#include <X11/Xft/Xft.h>`
- **Updated Font Arrays**: Changed `XFontStruct *FontInfo[32]` to `XftFont *FontInfo[32]`
- **Updated Utility Functions**:
  - `GetTextWidth()`: Uses `XftTextExtentsUtf8()` for accurate text measurement
  - `GetFontSize()`: Uses XftFont metrics (`max_advance_width`, `height`)
- **Updated Font Loading**: Uses `XftFontOpenName()` with scalable font names

### 1.5. **Build System**
- **CMakeLists.txt**: Xft is already properly configured and linked
- Required libraries: `Xft`, `Xmu`, `Xpm`, `Xrender`, `Xt`, `Freetype`, `Fontconfig`

---

## 2. Buffer Safety Modernization

### 2.1. **Replaced Unsafe String Functions**
- **All `sprintf` calls replaced with `snprintf`**
  - Ensures all formatted string writes are bounded by buffer size
- **All `strcat` calls replaced with `strncat`**
  - Prevents buffer overflows during string concatenation
- **Audit covered all user code** (main, term, zone, loader, socket, etc.)
- **Test code and third-party code left unchanged**

### 2.2. **Files Modified for Buffer Safety**
- `main/check.cc`, `main/credit.cc`, `main/customer.cc`, `main/employee.cc`, `main/printer.cc`, `main/settings.cc`, `main/license_hash.cc`
- `loader/loader_main.cc`, `socket.cc`
- All major files in `zone/` (e.g., `dialog_zone.cc`, `payment_zone.cc`, `report_zone.cc`, `user_edit_zone.cc`, `zone.cc`)
- **All string formatting and concatenation in user code is now safe**

### 2.3. **Security Benefits**
- Eliminates buffer overflow vulnerabilities from string formatting and concatenation
- Improves code robustness and maintainability
- Lays groundwork for future migration to `std::string` and modern C++

---

## Benefits

### 1. **Scalability & Modern Font Support**
- Fonts can be rendered at any size without quality loss
- Support for TrueType, OpenType, and other modern font formats
- Better font hinting and anti-aliasing
- Consistent rendering across different platforms

### 2. **Buffer Safety & Security**
- All string formatting and concatenation is now bounds-checked
- No more risk of buffer overflows from `sprintf`/`strcat`
- Improved reliability and security for all user input and output

### 3. **Preserved Functionality**
- **Embossed Text Effect**: The three-pass rendering algorithm is maintained
- **Color Support**: Full color support with proper XftColor handling
- **Text Measurement**: Accurate text width calculation for layout

### 4. **Backward Compatibility**
- Fallback to default font if scalable fonts fail to load
- Maintains existing font ID system
- No changes required to UI code

---

## Font Specifications

The migration uses modern font specifications:

| Old Bitmapped Font | New Scalable Font |
|-------------------|-------------------|
| `-adobe-times-medium-r-normal--24-*-p-*` | `Times:size=24:style=regular` |
| `-adobe-times-bold-r-normal--34-*-p-*` | `Times:size=34:style=bold` |
| `-adobe-courier-medium-r-normal--18-*-*-*-*-*-*-*` | `Courier:size=18:style=regular` |

---

## Testing

### 1. **Build Testing**
```bash
cd /home/ariel/Documents/Repos/viewtouchFork
cmake -B build .
cmake --build build
```

### 2. **Font Loading Test**
- Verify all fonts load without errors
- Check fallback behavior if fonts are missing
- Test different font sizes

### 3. **Text Rendering Test**
- Verify embossed text effect works correctly
- Test text alignment (left, center, right)
- Check text measurement accuracy

### 4. **UI Testing**
- Test Button Properties Dialog font selection
- Test Multi-Button Property Dialog
- Verify text rendering in all zones

### 5. **Buffer Safety Test**
- Fuzz test all user input fields for string overflows
- Review all formatted output for truncation handling
- Confirm no warnings or errors from static analysis tools (e.g., `-Wformat-truncation`)

---

## Future Enhancements

### 1. **Font Selection UI**
- Update dialogs to show scalable font options
- Add font preview functionality
- Support for custom font families

### 2. **Dynamic Font Sizing**
- Support for runtime font size changes
- Automatic font scaling based on screen resolution
- User-configurable font sizes

### 3. **Advanced Typography**
- Support for font features (ligatures, kerning)
- Better text layout with HarfBuzz
- Support for right-to-left languages

### 4. **Further Code Modernization**
- Migrate to `std::string` and modern C++ idioms
- Add more unit tests and static analysis
- Continue to audit for legacy C-style code

---

## Troubleshooting

### Common Issues

1. **Font Not Found**
   - Check if font is installed: `fc-list | grep Times`
   - Verify fontconfig configuration: `fc-match Times`
   - Use fallback font specification

2. **Build Errors**
   - Ensure Xft development packages are installed
   - Check CMake configuration for Xft libraries
   - Verify include paths are correct

3. **Rendering Issues**
   - Check XftColor allocation
   - Verify display connection
   - Test with different font sizes

4. **String Truncation**
   - If output is unexpectedly truncated, review buffer sizes and usages
   - Increase buffer sizes if necessary, or migrate to `std::string`

5. **vt_data Loading Issues**
   - **Expected Behavior**: ViewTouch will load but may show missing areas, empty screens, or incomplete functionality
   - **Cause**: The `vt_data` files contain essential business data (menus, items, settings, etc.) that are not included in the source code
   - **Solution**: Download the required data files from https://www.viewtouch.com/vt_data.html
   - **Note**: This is normal for development/testing environments without the full business data set

### Debugging

Enable debug output to see font loading:
```bash
export FC_DEBUG=1
./vtpos
```

---

## Conclusion

The migration to scalable fonts and buffer-safe string handling successfully modernizes ViewTouch's text rendering and core logic. The implementation uses industry-standard Xft and safe C string functions, providing a solid foundation for future enhancements in typography, security, and code quality.

---

## Files Modified

- `term/layer.cc` - Core text rendering
- `term/term_view.cc` - Font system
- `term/term_view.hh` - Header declarations
- `main/manager.cc` - Manager functions
- `main/check.cc`, `main/credit.cc`, `main/customer.cc`, `main/employee.cc`, `main/printer.cc`, `main/settings.cc`, `main/license_hash.cc`
- `loader/loader_main.cc`, `socket.cc`
- All major files in `zone/` (e.g., `dialog_zone.cc`, `payment_zone.cc`, `report_zone.cc`, `user_edit_zone.cc`, `zone.cc`)
- `CMakeLists.txt` - Build configuration (already configured)

## Dependencies

- Xft (X FreeType interface)
- FreeType (font rendering library)
- Fontconfig (font configuration)
- X11 development libraries 