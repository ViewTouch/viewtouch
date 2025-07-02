# ViewTouch Xft Font System Migration

This document describes the migration from legacy bitmapped fonts to the modern Xft (X FreeType) scalable font system in ViewTouch.

## Installing Fonts for Xft / Fontconfig Systems

### 📁 Where Xft Gets Fonts

Xft doesn't have its own font path. Instead, it uses **Fontconfig**, which automatically scans these locations:

- **System-wide** fonts:  
  - `/usr/share/fonts/`  
  - `/usr/local/share/fonts/`  

- **User-local** fonts:  
  - `~/.fonts/` (deprecated but still supported)  
  - `~/.local/share/fonts/` (modern Fontconfig default)

To verify what fonts are available:
```bash
fc-list | grep -i "bookman"
```

## Font Installation Process

### System-wide Installation

1. **Create font directory** (if it doesn't exist):
   ```bash
   sudo mkdir -p /usr/local/share/fonts/truetype
   ```

2. **Copy font files** to the directory:
   ```bash
   sudo cp your-font.ttf /usr/local/share/fonts/truetype/
   ```

3. **Update font cache**:
   ```bash
   sudo fc-cache -fv
   ```

### User-local Installation

1. **Create user font directory**:
   ```bash
   mkdir -p ~/.local/share/fonts
   ```

2. **Copy font files**:
   ```bash
   cp your-font.ttf ~/.local/share/fonts/
   ```

3. **Update user font cache**:
   ```bash
   fc-cache -fv
   ```

## Supported Font Formats

Fontconfig supports multiple font formats:
- **TrueType** (`.ttf`, `.ttc`)
- **OpenType** (`.otf`)
- **Type 1** (`.pfa`, `.pfb`)
- **Bitmap fonts** (`.bdf`, `.pcf`)

## Troubleshooting

### Check Available Fonts
```bash
fc-list | head -20
```

### Check Font Configuration
```bash
fc-match "Arial"
```

### Debug Font Loading
```bash
FC_DEBUG=1 fc-match "Your Font Name"
```

### Verify Font Installation
```bash
fc-cache -v | grep "your-font"
```

## ViewTouch Font Configuration

ViewTouch supports multiple font families:

### Default Fonts
- **Times** (serif) - Legacy compatibility
- **DejaVu Sans** (sans-serif) - Modern default
- **DejaVu Sans Mono** (monospace) - For numbers and prices

### Classic Serif Fonts (New)
- **EB Garamond 8** - Elegant serif font with excellent readability
- **Bookman** - Warm, readable serif font
- **Nimbus Roman** - Clean, professional serif font

### Font Usage in Code
```cpp
// Example usage of new fonts
ZoneText("Welcome", 0, 0, width, height, COLOR_BLACK, FONT_GARAMOND_24, ALIGN_CENTER);
ZoneText("Menu Items", 0, 0, width, height, COLOR_BLACK, FONT_BOOKMAN_18, ALIGN_LEFT);
ZoneText("$12.99", 0, 0, width, height, COLOR_BLACK, FONT_NIMBUS_20, ALIGN_RIGHT);
```

### Installing the New Fonts

#### EB Garamond 8
```bash
# Download from Google Fonts
wget https://github.com/google/fonts/raw/main/ofl/ebgaramond/EBGaramond-Regular.ttf
sudo cp EBGaramond-Regular.ttf /usr/local/share/fonts/truetype/
sudo fc-cache -fv
```

#### Bookman
```bash
# Install from package manager (if available)
sudo apt-get install fonts-urw-base35  # Includes Bookman
# Or manually install Bookman font files
```

#### Nimbus Roman
```bash
# Install from package manager
sudo apt-get install fonts-urw-base35  # Includes Nimbus Roman
# Or manually install Nimbus Roman font files
```

To use custom fonts, ensure they are properly installed in one of the Fontconfig directories listed above, then restart ViewTouch for the changes to take effect. 