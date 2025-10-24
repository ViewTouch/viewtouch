#!/bin/bash
# ViewTouch Universal Linux Installer Creator
# This script creates a self-extracting universal installer

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/installer-build"
INSTALLER_NAME="ViewTouch-Universal-Installer.run"

echo "Creating ViewTouch Universal Linux Installer..."
echo "Project root: $PROJECT_ROOT"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create the installer payload (source code + scripts)
echo "Preparing installer payload..."
cd "$PROJECT_ROOT"

# Create source archive (exclude git, build artifacts, etc.)
tar czf "$BUILD_DIR/viewtouch-source.tar.gz" \
    --exclude='.git*' \
    --exclude='build' \
    --exclude='installer-build' \
    --exclude='*.AppImage' \
    --exclude='appimagetool*' \
    --exclude='squashfs-root' \
    .

# Get source size for the installer
SOURCE_SIZE=$(wc -c < "$BUILD_DIR/viewtouch-source.tar.gz")

# Create the main installer script
cat > "$BUILD_DIR/installer-main.sh" << 'INSTALLER_EOF'
#!/bin/bash
# ViewTouch Universal Linux Installer
# Auto-detects Linux distribution and installs ViewTouch

set -euo pipefail

# Installer configuration
VIEWTOUCH_VERSION="25.02.0"
INSTALL_PREFIX="/usr/viewtouch"
TEMP_DIR="/tmp/viewtouch-install-$$"

# Save the installer script path before any directory changes
INSTALLER_SCRIPT="$(readlink -f "$0")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This installer must be run as root or with sudo"
        log_info "Please run: sudo $0"
        exit 1
    fi
}

# Detect Linux distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO_ID="$ID"
        DISTRO_NAME="$NAME"
        DISTRO_VERSION="${VERSION_ID:-unknown}"
    else
        log_error "Cannot detect Linux distribution"
        exit 1
    fi
    
    log_info "Detected: $DISTRO_NAME $DISTRO_VERSION"
}

# Detect package manager and set commands
setup_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        PKG_MANAGER="apt"
        PKG_UPDATE="apt-get update"
        PKG_INSTALL="apt-get install -y"
        PACKAGES="build-essential cmake git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev libfreetype6-dev libfontconfig1-dev zlib1g-dev libmotif-dev libcurl4-openssl-dev pkg-config"
    elif command -v dnf >/dev/null 2>&1; then
        PKG_MANAGER="dnf"
        PKG_UPDATE="dnf check-update || true"
        PKG_INSTALL="dnf install -y"
        PACKAGES="gcc gcc-c++ cmake git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel freetype-devel fontconfig-devel zlib-devel motif-devel libcurl-devel pkgconfig"
    elif command -v yum >/dev/null 2>&1; then
        PKG_MANAGER="yum"
        PKG_UPDATE="yum check-update || true"
        PKG_INSTALL="yum install -y"
        PACKAGES="gcc gcc-c++ cmake git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel freetype-devel fontconfig-devel zlib-devel motif-devel libcurl-devel pkgconfig"
    elif command -v pacman >/dev/null 2>&1; then
        PKG_MANAGER="pacman"
        PKG_UPDATE="pacman -Sy"
        PKG_INSTALL="pacman -S --noconfirm"
        PACKAGES="base-devel cmake git libx11 libxft libxmu libxpm libxrender libxt freetype2 fontconfig zlib motif libcurl-gnutls pkgconf"
    elif command -v zypper >/dev/null 2>&1; then
        PKG_MANAGER="zypper"
        PKG_UPDATE="zypper refresh"
        PKG_INSTALL="zypper install -y"
        PACKAGES="gcc gcc-c++ cmake git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel freetype2-devel fontconfig-devel zlib-devel motif-devel libcurl-devel pkg-config"
    else
        log_error "No supported package manager found (apt, dnf, yum, pacman, zypper)"
        exit 1
    fi
    
    log_info "Using package manager: $PKG_MANAGER"
}

# Install dependencies
install_dependencies() {
    log_info "Installing build dependencies..."
    
    log_info "Updating package database..."
    $PKG_UPDATE
    
    log_info "Installing packages: $PACKAGES"
    $PKG_INSTALL $PACKAGES
    
    log_success "Dependencies installed successfully"
}

# Extract source code
extract_source() {
    log_info "Extracting ViewTouch source code..."
    log_info "Original installer location: $INSTALLER_SCRIPT"
    
    # Verify the installer file exists
    if [ ! -f "$INSTALLER_SCRIPT" ]; then
        log_error "Cannot find installer script at: $INSTALLER_SCRIPT"
        log_error "Current working directory: $(pwd)"
        log_error "Script argument was: $0"
        exit 1
    fi
    
    mkdir -p "$TEMP_DIR"
    cd "$TEMP_DIR"
    
    # Find the archive marker (force text mode with -a to handle binary content)
    ARCHIVE_LINE=$(grep -a -n "^__SOURCE_ARCHIVE__$" "$INSTALLER_SCRIPT" | cut -d: -f1)
    if [ -z "$ARCHIVE_LINE" ]; then
        log_error "Cannot find source archive marker in installer"
        log_error "Checked file: $INSTALLER_SCRIPT"
        log_error "File size: $(wc -c < "$INSTALLER_SCRIPT") bytes"
        
        # Debug: show what grep actually finds
        log_info "Attempting to find marker with different methods..."
        if grep -a "__SOURCE_ARCHIVE__" "$INSTALLER_SCRIPT" >/dev/null; then
            log_info "Marker exists but line number detection failed"
            # Try alternative method using awk
            ARCHIVE_LINE=$(awk '/^__SOURCE_ARCHIVE__$/{print NR; exit}' "$INSTALLER_SCRIPT")
            if [ -n "$ARCHIVE_LINE" ]; then
                log_info "Found marker at line $ARCHIVE_LINE using awk"
            fi
        else
            log_error "Marker '__SOURCE_ARCHIVE__' not found in file at all"
        fi
        
        if [ -z "$ARCHIVE_LINE" ]; then
            exit 1
        fi
    fi
    
    SOURCE_LINE=$((ARCHIVE_LINE + 1))
    log_info "Archive starts at line $SOURCE_LINE"
    
    # Extract the embedded source archive
    if ! tail -n +"$SOURCE_LINE" "$INSTALLER_SCRIPT" | tar xz; then
        log_error "Failed to extract source archive"
        log_info "Installer file: $INSTALLER_SCRIPT"
        log_info "Archive starts at line: $SOURCE_LINE"
        log_info "File size: $(wc -c < "$INSTALLER_SCRIPT") bytes"
        exit 1
    fi
    
    log_success "Source code extracted to $TEMP_DIR"
}

# Build ViewTouch
build_viewtouch() {
    log_info "Building ViewTouch..."
    cd "$TEMP_DIR"
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure with CMake
    log_info "Configuring build..."
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="/usr" ..
    
    # Build
    log_info "Compiling... (this may take a few minutes)"
    make -j$(nproc)
    
    log_success "Build completed successfully"
}

# Install ViewTouch
install_viewtouch() {
    log_info "Installing ViewTouch to $INSTALL_PREFIX..."
    cd "$TEMP_DIR/build"
    
    # Create install directory
    mkdir -p "$INSTALL_PREFIX"
    
    # Install
    make install
    
    # Set proper permissions
    chown -R root:root "$INSTALL_PREFIX"
    chmod -R 755 "$INSTALL_PREFIX"
    
    # Make binaries executable (if they exist)
    if [ -d "$INSTALL_PREFIX/bin" ] && [ "$(ls -A "$INSTALL_PREFIX/bin" 2>/dev/null)" ]; then
        chmod +x "$INSTALL_PREFIX/bin/"*
        log_info "Set executable permissions for binaries"
    else
        log_warning "No binaries found in $INSTALL_PREFIX/bin"
    fi
    
    # Create desktop entry
    create_desktop_entry
    
    # Create symlinks for easy access
    create_symlinks
    
    log_success "ViewTouch installed successfully"
}

# Create desktop entry
create_desktop_entry() {
    log_info "Creating desktop entry..."
    
    cat > /usr/share/applications/viewtouch.desktop << EOF
[Desktop Entry]
Type=Application
Name=ViewTouch POS
Comment=Point of Sale System
Exec=$INSTALL_PREFIX/bin/vtpos
Icon=$INSTALL_PREFIX/share/pixmaps/viewtouch.png
Categories=Office;Finance;
Terminal=false
StartupNotify=true
EOF
    
    # Copy icon if it exists (check both old and new locations)
    ICON_FILE=""
    if [ -f "$TEMP_DIR/assets/images/Icon.png" ]; then
        ICON_FILE="$TEMP_DIR/assets/images/Icon.png"
    elif [ -f "$TEMP_DIR/xpm/Icon.png" ]; then
        ICON_FILE="$TEMP_DIR/xpm/Icon.png"
    fi
    
    if [ -n "$ICON_FILE" ]; then
        mkdir -p "$INSTALL_PREFIX/share/pixmaps"
        cp "$ICON_FILE" "$INSTALL_PREFIX/share/pixmaps/viewtouch.png"
        log_info "Desktop icon copied"
    fi
    
    log_info "Desktop entry created"
}

# Create convenient symlinks
create_symlinks() {
    log_info "Creating system symlinks..."
    
    # Create symlink in /usr/local/bin for easy command line access
    ln -sf "$INSTALL_PREFIX/bin/vtpos" /usr/local/bin/vtpos
    ln -sf "$INSTALL_PREFIX/bin/vt_main" /usr/local/bin/vt_main
    
    log_info "Created symlinks in /usr/local/bin"
}

# Cleanup
cleanup() {
    log_info "Cleaning up temporary files..."
    rm -rf "$TEMP_DIR"
    log_info "Cleanup completed"
}

# Main installation function
main() {
    echo ""
    echo "=================================================="
    echo "    ViewTouch Universal Linux Installer v$VIEWTOUCH_VERSION"
    echo "=================================================="
    echo ""
    
    check_root
    detect_distro
    setup_package_manager
    
    # Ask for confirmation
    echo "This installer will:"
    echo "  1. Install build dependencies using $PKG_MANAGER"
    echo "  2. Build ViewTouch from source"
    echo "  3. Install ViewTouch to $INSTALL_PREFIX"
    echo "  4. Create desktop entry and system symlinks"
    echo ""
    read -p "Continue with installation? (y/N): " -n 1 -r
    echo ""
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Installation cancelled by user"
        exit 0
    fi
    
    echo ""
    log_info "Starting ViewTouch installation..."
    
    install_dependencies
    extract_source
    build_viewtouch
    install_viewtouch
    cleanup
    
    echo ""
    echo "=================================================="
    log_success "ViewTouch installation completed!"
    echo "=================================================="
    echo ""
    echo "You can now run ViewTouch in the following ways:"
    echo "  • From Applications menu: ViewTouch POS"
    echo "  • From terminal: vtpos"
    echo "  • Direct path: $INSTALL_PREFIX/bin/vtpos"
    echo ""
    echo "For support, visit: https://github.com/ViewTouch/viewtouch"
    echo ""
}

# Set trap for cleanup on exit
trap cleanup EXIT

# Run main installation
main "$@"

# Exit here - the source archive follows
exit 0

__SOURCE_ARCHIVE__
INSTALLER_EOF

# No need to replace line numbers - the script will find the marker dynamically

# Combine installer script and source archive
cat "$BUILD_DIR/installer-main.sh" "$BUILD_DIR/viewtouch-source.tar.gz" > "$BUILD_DIR/$INSTALLER_NAME"

# Make installer executable
chmod +x "$BUILD_DIR/$INSTALLER_NAME"

# Calculate final size
INSTALLER_SIZE=$(ls -lh "$BUILD_DIR/$INSTALLER_NAME" | awk '{print $5}')

echo ""
echo "=================================================="
echo "Universal Installer Created Successfully!"
echo "=================================================="
echo "File: $BUILD_DIR/$INSTALLER_NAME"
echo "Size: $INSTALLER_SIZE"
echo ""
echo "This installer will work on:"y
echo "  ✓ Fedora/RHEL/CentOS (dnf/yum)"
echo "  ✓ Arch Linux (pacman)"
echo "  ✓ openSUSE (zypper)"
echo "  ✓ Any Linux with supported package manager"
echo ""
echo "Users can run it with:"
echo "  sudo ./$INSTALLER_NAME"
echo ""
echo "Or make it double-clickable by setting executable permissions."
echo "=================================================="
