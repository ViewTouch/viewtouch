# ARM64 (aarch64) Support for ViewTouch

## Current Status

ViewTouch **fully supports ARM64 architecture** with our **Universal Linux Installer** that works seamlessly on ARM64 systems including Raspberry Pi.

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

## Universal Installer for ARM64

The easiest way to install ViewTouch on ARM64 systems is using our Universal Linux Installer:

```bash
# Download the universal installer
wget https://github.com/No0ne558/viewtouchFork/releases/latest/download/ViewTouch-Universal-Installer.run

# Make it executable and install
chmod +x ViewTouch-Universal-Installer.run
sudo ./ViewTouch-Universal-Installer.run
```

The universal installer will:
- Auto-detect ARM64 architecture
- Install dependencies using the system package manager
- Compile ViewTouch optimized for your specific ARM64 system
- Create desktop entries and system integration

## Distribution

The universal installer is automatically built by GitHub Actions and works on:
- **Raspberry Pi OS** (64-bit)
- **Ubuntu** (ARM64)
- **Debian** (ARM64) 
- **Fedora** (ARM64)
- **Arch Linux** (ARM64)
- Any other ARM64 Linux distribution
## Technical Details

The universal installer approach provides several advantages for ARM64:

- **Native compilation**: Builds are optimized for the specific ARM64 system
- **Automatic dependencies**: System package manager handles ARM64 X11 libraries  
- **Better integration**: Proper desktop entries and system-wide installation
- **No special requirements**: Works on containers and restricted environments

This approach ensures maximum compatibility and performance on ARM64 systems.

## Package Support

For manual package creation, ViewTouch supports:

- **DEB packages**: `cpack -G DEB` (on ARM64 system)
- **RPM packages**: `cpack -G RPM` (on ARM64 system) 
- **Direct installation**: Works on all ARM64 Linux distributions

The codebase is fully ARM64-compatible and has been tested on Raspberry Pi systems.
