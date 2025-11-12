# ViewTouch Development Workflow

This document outlines the clean development workflow for ViewTouch after the recent directory reorganization.

## Directory Structure

```
viewtouchFork/
├── src/                    # Organized source code
│   ├── core/              # Core functionality (data, config, logging)
│   ├── utils/             # Utility functions and helpers
│   └── network/           # Network-related code (sockets, remote links)
├── main/                  # Main application modules
├── zone/                  # UI zone components
├── term/                  # Terminal/display components
├── cdu/                   # Customer Display Unit
├── print/                 # Printing functionality
├── loader/                # Application loader
├── version/               # Version management
├── fonts/                 # Organized font files
│   ├── dejavu/           # DejaVu font family
│   ├── liberation/       # Liberation font family
│   ├── urw/              # URW font family
│   ├── nimbus/           # Nimbus font family
│   └── ebgaramond/       # EB Garamond font family
├── external/              # External dependencies
├── cmake/                 # CMake configuration files
├── scripts/               # Build and utility scripts
├── docs/                  # Documentation
├── android/               # Android-specific code
├── packaging/             # Packaging and installer scripts
├── po_file/               # Language/translation files
└── xpm/                   # XPM image files
```

## Build Process

### Prerequisites
- CMake 3.5.1 or higher
- C++20 compatible compiler (GCC 12+, Clang 16+)
- X11 development libraries
- Motif development libraries
- Freetype development libraries
- Fontconfig development libraries

### Building
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Install (requires sudo)
sudo make install
```

### Running
```bash
# Start ViewTouch (automatically opens vt_term)
sudo /usr/viewtouch/bin/vtpos
```

## Development Guidelines

### Code Organization
- **Core functionality** goes in `src/core/`
- **Utility functions** go in `src/utils/`
- **Network code** goes in `src/network/`
- **Main application logic** stays in `main/`
- **UI components** stay in `zone/`

### File Naming
- Use descriptive names for source files
- Keep header and source files paired (.cc/.hh)
- Use snake_case for file names

### CMake Integration
- All new source files must be added to appropriate CMakeLists.txt targets
- Include directories are automatically configured for the new structure
- External dependencies are managed in the external/ directory

### Testing
- Test code should be placed in a `tests/` directory (when created)
- Use Catch2 v3 for unit testing
- Build tests with `cmake -DBUILD_TESTING=ON ..`

## Clean Development Practices

### Git Workflow
- Use feature branches for development
- Commit working code only after testing
- Use descriptive commit messages

### Build Artifacts
- Build directory is ignored by git
- Generated files are excluded via .gitignore
- Clean builds recommended for testing

### Font Management
- Fonts are organized by family in subdirectories
- Archive files are excluded from the repository
- Use EB Garamond 14 Bold (FONT_GARAMOND_14B) as standard UI font

## Troubleshooting

### Build Issues
1. Clean build directory: `rm -rf build && mkdir build`
2. Check dependencies: `apt list --installed | grep -E "(xorg-dev|libmotif-dev|libfreetype6-dev)"`
3. Verify CMake version: `cmake --version`

### Runtime Issues
1. Check permissions: ViewTouch requires sudo for system-wide operation
2. Verify X11 display: `echo $DISPLAY`
3. Check font availability: Fonts are bundled in the fonts/ directory

## Contributing

1. Follow the established directory structure
2. Update CMakeLists.txt for new source files
3. Test builds before committing
4. Document significant changes
5. Use the project's coding standards (C++20, modern practices)

## Memory Management

The project uses modern C++17/20 features including:
- Smart pointers for memory safety
- RAII principles
- Automatic resource management
- Exception safety

Avoid raw pointers and manual memory management where possible.

## Related Documentation

- **Payment Processor Integration**: `PAYMENT_PROCESSOR_INTEGRATION.md` - Future planning/idea documentation for potential Stripe Terminal integration with Android devices and thermal printers (not a confirmed implementation plan)
- **Modern C++**: `MODERN_CPP.md` - Guidelines for modern C++ practices
- **Memory Modernization**: `MEMORY_MODERNIZATION.md` - Memory management improvements
- **String Safety**: `STRING_SAFETY.md` - String handling best practices
- **Testing**: `TESTING.md` - Testing guidelines and practices
