# ARM64 (aarch64) Support for ViewTouch

## Current Status

ViewTouch **fully supports ARM64 architecture** but the GitHub Actions AppImage build for ARM64 is temporarily disabled due to cross-compilation complexity with X11 dependencies.

## Running on ARM64 Systems

ViewTouch works perfectly on ARM64 systems including:
- **Raspberry Pi 4/5** with 64-bit Raspberry Pi OS
- **ARM-based servers** (AWS Graviton, etc.)
- **Apple Silicon Macs** via Docker/Linux VMs
- **Other ARM64 Linux systems**

## Building Locally on ARM64

On any ARM64 Linux system, build ViewTouch natively:

```bash
# Clone the repository
git clone https://github.com/No0ne558/viewtouchFork.git
cd viewtouchFork

# Install dependencies (Debian/Ubuntu-based)
sudo apt-get update
sudo apt-get install -y build-essential cmake git \
  libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev \
  libfreetype6-dev libfontconfig1-dev zlib1g-dev \
  libmotif-dev libcurl4-openssl-dev pkg-config

# Build ViewTouch
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install locally
sudo cmake --install build

# Run ViewTouch
sudo /usr/viewtouch/bin/vtpos
```

## Creating ARM64 AppImage

On an ARM64 system, you can create a portable AppImage:

```bash
# After building (see above), stage AppDir
rm -rf build/AppDir && DESTDIR=build/AppDir cmake --install build
install -Dm755 packaging/appimage/AppRun build/AppDir/AppRun
install -Dm644 packaging/appimage/viewtouch.desktop build/AppDir/viewtouch.desktop
cp xpm/demo.png build/AppDir/viewtouch.png

# Download ARM64 appimagetool
wget -O appimagetool https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-aarch64.AppImage
chmod +x appimagetool

# Create AppImage
APPIMAGE_EXTRACT_AND_RUN=1 ./appimagetool build/AppDir
```

## Future Plans

We plan to add ARM64 AppImage builds using:
1. **Self-hosted ARM runners** on GitHub Actions
2. **Docker-based cross-compilation** with proper X11 library mapping
3. **Native ARM64 CI/CD** environments

## Native Package Support

ViewTouch supports native ARM64 packages:

- **DEB packages**: `cpack -G DEB` (on ARM64 system)
- **RPM packages**: `cpack -G RPM` (on ARM64 system)
- **Direct installation**: Works on all ARM64 Linux distributions

The codebase is fully ARM64-compatible and has been tested on Raspberry Pi systems.

## Why Not Cross-Compile?

Cross-compiling ViewTouch for ARM64 from x86_64 is complex because:
- **X11 dependencies**: Requires ARM64 versions of libX11, libXft, Motif, etc.
- **Library linking**: CMake's find_package struggles with cross-compiled X11 libraries
- **Testing limitations**: Cannot test ARM64 AppImages on x86_64 runners

Native compilation on ARM64 systems is the most reliable approach.
