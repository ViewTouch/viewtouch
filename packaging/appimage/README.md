# ViewTouch AppImage - Universal Linux Package

This folder contains the AppImage packaging system for ViewTouch POS, designed to work on **all Linux distributions**.

## 🌍 Universal Compatibility

The AppImage works on:
- **x86_64 systems**: Intel/AMD 64-bit processors
- **aarch64 systems**: ARM 64-bit processors (Raspberry Pi 4/5, ARM servers)
- **All distributions**: Ubuntu, Debian, Fedora, CentOS, openSUSE, Arch, Raspberry Pi OS, etc.
- **Old and new**: No specific glibc/library version requirements

## 📦 Download & Run

1. Download the appropriate AppImage:
   - `ViewTouch-x86_64.AppImage` for Intel/AMD systems
   - `ViewTouch-aarch64.AppImage` for ARM systems (Raspberry Pi, etc.)

2. Make executable and run:
   ```bash
   chmod +x ViewTouch-*.AppImage
   ./ViewTouch-*.AppImage
   ```

3. Or double-click in file manager!

## 🔧 How It Works

The AppImage bundles:
- ViewTouch binaries (`vtpos`, `vt_main`, `vt_term`, etc.)
- Required fonts (DejaVu, EB Garamond, Liberation, etc.)
- Essential X11/graphics libraries
- Language packs
- Configuration files

## 🏗️ Building Locally

```bash
# Build ViewTouch
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Stage AppDir
rm -rf build/AppDir && DESTDIR=build/AppDir cmake --install build
install -Dm755 packaging/appimage/AppRun build/AppDir/AppRun
install -Dm644 packaging/appimage/viewtouch.desktop build/AppDir/viewtouch.desktop
cp xpm/demo.png build/AppDir/viewtouch.png

# Create AppImage (requires appimagetool)
wget -O appimagetool https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-$(uname -m).AppImage
chmod +x appimagetool
APPIMAGE_EXTRACT_AND_RUN=1 ./appimagetool build/AppDir
```

## 🚀 Features

- **Self-contained**: No installation required
- **Portable**: Run from USB drive or any location  
- **Sandboxed**: Doesn't interfere with system
- **Universal**: Single file works everywhere
- **Cross-architecture**: x86_64 and ARM64 support

Perfect for restaurants, retail stores, and POS deployments across diverse Linux environments!
