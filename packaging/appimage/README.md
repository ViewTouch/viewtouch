# ViewTouch AppImage Packaging

This directory contains the files needed to create a ViewTouch AppImage - a portable, single-file installer optimized for Debian-based Linux distributions.

## Target Systems

**Optimized for Debian-based distributions:**
- Debian (10, 11, 12+)
- Ubuntu (18.04, 20.04, 22.04, 24.04+)
- Linux Mint
- Pop!_OS
- Elementary OS
- Other Debian derivatives

## Files

- **AppRun**: The main launcher script that handles ViewTouch's hardcoded paths
- **viewtouch.desktop**: Desktop entry file for the application
- **README.md**: This documentation file

## Creating the AppImage

1. Build ViewTouch normally to create `build/AppDir/`
2. Use AppImageTool to package it:
   ```bash
   ./appimagetool-x86_64.AppImage build/AppDir/ ViewTouch-$(date +%Y%m%d)-debian-x86_64.AppImage
   ```

## How it Works

ViewTouch has hardcoded paths to `/usr/viewtouch` compiled into the binary. The AppRun script solves this by:

1. Creating a temporary writable copy of ViewTouch data
2. Using sudo to create system symlinks when available  
3. Falling back to user namespaces if sudo isn't available
4. Automatic cleanup on exit

## Debian Optimization

The AppImage includes optimized library bundling for Debian systems:

- **X11 Libraries**: libXft, libXmu, libXpm, libXrender, libXt, libX11
- **Font Rendering**: libfontconfig, libfreetype
- **Motif UI**: libXm for proper GUI rendering
- **Network**: libcurl, libssl, libcrypto for secure connections

## Usage

Users can simply:
1. Download the `ViewTouch-*-debian-x86_64.AppImage` file
2. Make it executable: `chmod +x ViewTouch-*-debian-x86_64.AppImage`
3. Run it: `./ViewTouch-*-debian-x86_64.AppImage`
4. Provide sudo password when prompted (for system symlink creation)

The AppImage is self-contained and works across all Debian-based distributions without additional dependencies.