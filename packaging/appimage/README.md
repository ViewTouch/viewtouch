# ViewTouch AppImage Packaging

This directory contains the files needed to create a ViewTouch AppImage - a portable, single-file installer for Linux.

## Files

- **AppRun**: The main launcher script that handles ViewTouch's hardcoded paths
- **viewtouch.desktop**: Desktop entry file for the application
- **README.md**: This documentation file

## Creating the AppImage

1. Build ViewTouch normally to create `build/AppDir/`
2. Use AppImageTool to package it:
   ```bash
   ./appimagetool-aarch64.AppImage build/AppDir/ ViewTouch-$(date +%Y%m%d)-aarch64.AppImage
   ```

## How it Works

ViewTouch has hardcoded paths to `/usr/viewtouch` compiled into the binary. The AppRun script solves this by:

1. Creating a temporary writable copy of ViewTouch data
2. Using sudo to create system symlinks when available  
3. Falling back to user namespaces if sudo isn't available
4. Automatic cleanup on exit

## Usage

Users can simply:
1. Download the `.AppImage` file
2. Make it executable: `chmod +x ViewTouch-*.AppImage`
3. Run it: `./ViewTouch-*.AppImage`
4. Provide sudo password when prompted (for system symlink creation)

The AppImage is self-contained and works on any modern Linux distribution.