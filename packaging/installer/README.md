# ViewTouch Universal Linux Installer

This creates a single `.run` file that works on **ANY Linux distribution** - just like commercial software installers (NVIDIA, VMware, etc.).

## What It Does

✅ **Universal compatibility** - Works on ANY Linux distro  
✅ **Auto-detects** package manager (apt, dnf, yum, pacman, zypper)  
✅ **Installs dependencies** automatically  
✅ **Builds from source** for optimal performance  
✅ **System-wide installation** to `/usr/viewtouch/`  
✅ **Creates desktop entry** and command-line shortcuts  
✅ **Single file** - easy to distribute

## Supported Linux Distributions

- **Debian-based**: Debian, Ubuntu, Linux Mint, Pop!_OS, Elementary OS
- **Red Hat-based**: Fedora, RHEL, CentOS, Rocky Linux, AlmaLinux  
- **Arch-based**: Arch Linux, Manjaro, EndeavourOS
- **SUSE-based**: openSUSE Leap, openSUSE Tumbleweed, SUSE Linux Enterprise
- **Any Linux** with a supported package manager

## Supported Architectures

- **Intel/AMD x86_64**: Desktop and server systems
- **ARM64**: Raspberry Pi 4, Pi 5, and compatible single-board computers

## Creating the Installer

Run this script to create the universal installer:

```bash
cd packaging/installer
./create-universal-installer.sh
```

This creates: `ViewTouch-Universal-Installer.run`

## For End Users

### Installation

1. **Download** the `ViewTouch-Universal-Installer.run` file
2. **Make executable**: `chmod +x ViewTouch-Universal-Installer.run`
3. **Run as root**: `sudo ./ViewTouch-Universal-Installer.run`

The installer will:
- Detect your Linux distribution
- Install build dependencies
- Build ViewTouch from source
- Install system-wide
- Create desktop shortcuts

### Usage After Installation

- **GUI**: Find "ViewTouch POS" in your Applications menu
- **Terminal**: Run `vtpos` from anywhere
- **Direct**: `/usr/viewtouch/bin/vtpos`

## Technical Details

The installer is a **self-extracting shell script** that:

1. **Detects distribution** using `/etc/os-release`
2. **Installs dependencies** using the native package manager
3. **Extracts source code** embedded in the installer
4. **Builds with CMake** for the target architecture
5. **Installs system-wide** with proper permissions
6. **Creates shortcuts** for easy access
7. **Cleans up** temporary files

## CI/CD Integration

The installer is automatically built by GitHub Actions:
- **Workflow**: `.github/workflows/universal-installer.yml`
- **Trigger**: Manual or git tags (`v*`)
- **Artifact**: `ViewTouch-Universal-Installer.run`

## File Size

The installer contains the complete ViewTouch source code and is approximately **15MB**.
