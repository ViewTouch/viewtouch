# ViewTouch Testing Framework

## Overview

ViewTouch now includes a comprehensive unit testing framework using Catch2 v3, integrated with CMake. The testing infrastructure provides a solid foundation for ensuring code quality and preventing regressions.

## Directory Structure

```
tests/
├── main_test.cc          # Test entry point
├── CMakeLists.txt        # Test build configuration
├── unit/                 # Unit tests
│   ├── test_check.cc     # Mock class and arithmetic tests
│   ├── test_settings.cc  # Settings validation tests
│   └── test_utility.cc   # Input validation and utility tests
└── mocks/                # Mock objects for testing
    ├── mock_terminal.hh/.cc  # Mock terminal implementation
    └── mock_settings.hh/.cc  # Mock settings implementation
```

## Building and Running Tests

### Prerequisites
- CMake 3.5.1+
- C++20 compatible compiler
- Catch2 v3 (automatically fetched via CMake)

### Build with Tests Enabled
```bash
cd build
cmake .. -DBUILD_TESTING=ON
make vt_tests
```

### Run Tests
```bash
./tests/vt_tests
```

### Test Output
```
Randomness seeded to: 682079502
===============================================================================
All tests passed (40 assertions in 7 test cases)
```

## Test Categories

### 1. Mock Classes (`[mocks]`)
- **MockTerminal**: Tests terminal interface methods
- **MockSettings**: Tests settings configuration and validation

### 2. Settings Validation (`[settings]`)
- Tax rate configuration and validation
- Drawer mode settings
- Input bounds checking

### 3. Input Validation (`[validation]`)
- Numeric input parsing
- String length limits
- Error handling for invalid inputs

### 4. Arithmetic Operations (`[arithmetic]`)
- POS calculation logic (taxes, totals, change)
- Payment processing math

## Writing New Tests

### Basic Test Structure
```cpp
#include <catch2/catch_all.hpp>

TEST_CASE("Test Category", "[category]")
{
    SECTION("Test subsection")
    {
        // Arrange
        // Act
        // Assert
        REQUIRE(result == expected);
    }
}
```

### Adding Mock Classes
Create new mocks in `tests/mocks/`:
```cpp
// mock_example.hh
class MockExample {
public:
    int GetValue() { return 42; }
    void SetValue(int val) { value_ = val; }
private:
    int value_ = 0;
};
```

### Integration Tests (Future)
```bash
# Planned structure
tests/integration/
├── test_database.cc
├── test_printer.cc
└── test_payment_processing.cc
```

## Test Coverage Goals

### Phase 1 (Current)
- ✅ Mock classes and basic functionality
- ✅ Settings validation
- ✅ Input sanitization
- ✅ Basic arithmetic operations

### Phase 2 (Next)
- Business logic (Check, Order, Payment processing)
- Database operations
- Network communication
- Hardware interfaces

### Phase 3 (Future)
- End-to-end workflow tests
- Performance regression tests
- Integration with external services

## Running Tests in CI/CD

### GitHub Actions Example
```yaml
- name: Build and Test
  run: |
    mkdir build && cd build
    cmake .. -DBUILD_TESTING=ON
    make vt_tests
    ./tests/vt_tests --reporter=junit > test-results.xml
```

### Test Reporting
```bash
# JUnit XML output for CI
./tests/vt_tests --reporter=junit

# Verbose output
./tests/vt_tests -v

# Run specific test
./tests/vt_tests "Mock classes functionality"
```

## Best Practices

### 1. Test Isolation
- Each test should be independent
- Use mocks to isolate dependencies
- Clean up test data between runs

### 2. Descriptive Names
- Use clear, descriptive test names
- Group related tests with tags
- Document complex test scenarios

### 3. Test Data
- Use realistic test data
- Cover edge cases and error conditions
- Test both valid and invalid inputs

### 4. Performance
- Keep tests fast (target < 100ms per test)
- Use fixtures for expensive setup
- Parallelize independent tests

## Mock Strategy

### Current Mocks
- **MockSettings**: Simplified settings without complex inheritance
- **MockTerminal**: Basic terminal interface for testing

### Future Mocks Needed
- **MockDatabase**: Database operations
- **MockPrinter**: Print job handling
- **MockPayment**: Payment processor interface
- **MockNetwork**: Socket communication

## Integration with Development Workflow

### Pre-commit Testing
```bash
#!/bin/bash
# Run tests before committing
cd build && make vt_tests && ./tests/vt_tests
if [ $? -ne 0 ]; then
    echo "Tests failed! Fix before committing."
    exit 1
fi
```

### Test-Driven Development
1. Write failing test for new feature
2. Implement feature until test passes
3. Refactor while maintaining test coverage
4. Add additional test cases

## Troubleshooting

### Common Issues

**Tests not building:**
- Ensure `-DBUILD_TESTING=ON` is set
- Check CMake version (3.5.1+ required)
- Verify C++20 compiler support

**Test failures:**
- Check test data and expectations
- Verify mock implementations
- Look for platform-specific issues

**Performance issues:**
- Profile slow tests with `--benchmark`
- Optimize test setup/teardown
- Consider test parallelization

## Future Enhancements

### 1. Code Coverage
- Integrate with gcov/lcov
- Set coverage targets (80%+)
- Generate coverage reports

### 2. Property-Based Testing
- Use Catch2 generators for random input testing
- Test edge cases automatically
- Validate mathematical properties

### 3. Fuzz Testing
- Integrate with libFuzzer
- Test input parsing robustness
- Find security vulnerabilities

### 4. Performance Testing
- Benchmark critical paths
- Regression detection
- Load testing capabilities

## Contributing

When adding new tests:

1. Follow the existing directory structure
2. Use descriptive test names and tags
3. Include both positive and negative test cases
4. Update this documentation
5. Ensure all tests pass before submitting

## Resources

- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [CMake Testing](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [Google Test Best Practices](https://google.github.io/googletest/primer.html)

---

**Test Status**: ✅ **40 assertions passing in 7 test cases**
**Last Updated**: October 30, 2025
