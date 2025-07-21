# Catch2 v2 to v3 Migration Guide

This document describes the migration from Catch2 v2.13.10 to Catch2 v3.8.1 in the ViewTouch project.

## Overview

Catch2 v3 represents a significant architectural change from v2. The main differences are:

- **v2**: Single header file (`catch.hpp`) - include and compile everything
- **v3**: Library-based approach - compile once, link against the library

## Migration Summary

### What Changed

1. **File Structure**: 
   - **v2**: Single `external/catch2/catch.hpp` file (17,977 lines)
   - **v3**: Modular headers in `external/catch2/src/catch2/` directory

2. **Build System**:
   - **v2**: No build configuration needed
   - **v3**: CMake subdirectory integration with library targets

3. **Usage**:
   - **v2**: `#include "external/catch2/catch.hpp"`
   - **v3**: `#include <catch2/catch_test_macros.hpp>` (and other specific headers)

### What Was Done

1. **Replaced Catch2 v2.13.10 with v3.8.1**
   - Downloaded Catch2 v3.8.1 from GitHub
   - Replaced `external/catch2/` directory
   - Cleaned up downloaded archive

2. **Updated CMakeLists.txt**
   - Added `add_subdirectory(external/catch2 EXCLUDE_FROM_ALL)`
   - Catch2 v3 library targets are available for future test development
   - Tests would use `Catch2::Catch2WithMain` target when created

3. **Test Infrastructure** (Removed)
   - Created and then removed `tests/basic/` directory as requested
   - Basic test and feature demonstration test were created to verify migration
   - Tests can be recreated in the future using the same patterns

## New Catch2 v3 Features

### Library Targets

- `Catch2::Catch2` - Main library (no main function)
- `Catch2::Catch2WithMain` - Library with main function included

### Header Organization

Instead of including everything with `catch.hpp`, you now include specific headers:

```cpp
// Basic test macros
#include <catch2/catch_test_macros.hpp>

// Approximate comparisons
#include <catch2/catch_approx.hpp>

// Generators
#include <catch2/generators/catch_generators.hpp>

// Matchers
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

// Or include everything (not recommended for compile times)
#include <catch2/catch_all.hpp>
```

### New Features Demonstrated

1. **Improved Matchers**:
   ```cpp
   REQUIRE_THAT(text, Catch::Matchers::StartsWith("Hello"));
   REQUIRE_THAT(value, Catch::Matchers::WithinRel(3.1416, 0.001));
   ```

2. **Enhanced Generators**:
   ```cpp
   auto test_data = GENERATE(
       std::make_pair(1, 1),
       std::make_pair(2, 4)
   );
   ```

3. **Table Generators**:
   ```cpp
   auto [a, b, expected] = GENERATE(
       table<int, int, int>({
           {1, 1, 2},
           {2, 3, 5}
       })
   );
   ```

## Building and Testing

### Build Configuration

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Running Tests (When Created)

```bash
# Run all tests
ctest

# Run specific test
./tests/basic/basic_test
./tests/basic/catch2_v3_features_test

# Run with verbose output
ctest -V
```

## Benefits of Migration

1. **Faster Compile Times**: Only compile what you need
2. **Better Organization**: Modular header structure
3. **Improved Features**: Enhanced matchers, generators, and reporting
4. **Future-Proof**: Active development and support
5. **Better Integration**: Proper CMake targets and modern C++ features

## Migration Checklist

- [x] Replace Catch2 v2 with v3.8.1
- [x] Update CMakeLists.txt integration
- [x] Create test infrastructure (verified migration)
- [x] Remove test infrastructure (as requested)
- [x] Verify build functionality
- [x] Document migration process
- [x] Demonstrate new features

## Next Steps

1. **Add More Tests**: Create tests for existing ViewTouch components
2. **Update Existing Tests**: If any existing tests exist, update them to use v3 syntax
3. **Configure Test Reporting**: Set up CI/CD integration for test reporting
4. **Performance Testing**: Leverage Catch2 v3's benchmarking capabilities

## Troubleshooting

### Common Issues

1. **Missing Headers**: Make sure to include the correct Catch2 v3 headers
2. **Link Errors**: Ensure you're linking against `Catch2::Catch2WithMain` for test executables
3. **CMake Errors**: Verify that `add_subdirectory(external/catch2 EXCLUDE_FROM_ALL)` is called

### Getting Help

- [Catch2 v3 Documentation](https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md)
- [Migration Guide](https://github.com/catchorg/Catch2/blob/devel/docs/migrate-v2-to-v3.md)
- [Catch2 v3.8.1 Release Notes](https://github.com/catchorg/Catch2/releases/tag/v3.8.1)

## Version Information

- **Previous Version**: Catch2 v2.13.10 (single header)
- **New Version**: Catch2 v3.8.1 (library-based)
- **Migration Date**: July 18, 2025
- **Migration Status**: ✅ Complete and Verified