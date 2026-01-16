# ViewTouch Development Branch

Welcome to the **dev** branch! This is a dedicated development environment for exploring, testing, and improving the ViewTouch codebase.

## What is This Branch?

The `dev` branch serves as an **experimental and testing ground** for new features, improvements, and refactoring. Unlike the `master` branch (which mirrors the official ViewTouch repository), the `dev` branch contains:

- Latest code changes and enhancements
- Work-in-progress features
- Community-driven improvements
- Modern C++ refactoring and modernizations
- Static analysis fixes and code quality improvements

## For Developers & Community Contributors

ViewTouch is **open to the community**! We welcome developers of all skill levels to:

✅ **Test your coding skills** — Improve and expand the ViewTouch codebase  
✅ **Learn by doing** — Work with a real POS system written in modern C++  
✅ **Use AI assistance** — Leverage tools like GitHub Copilot, ChatGPT, or Claude to accelerate development  
✅ **Contribute improvements** — Submit pull requests with enhancements, bug fixes, and optimizations  
✅ **Collaborate** — Join an active community dedicated to advancing ViewTouch

## ⚠️ Important Warning: Development Build Instability

**THIS IS A DEVELOPMENT BUILD** — Expect bugs, incomplete features, and breaking changes.

- **Frequent Changes**: Code is actively being modified and tested
- **Known Issues**: This branch will contain bugs and incomplete implementations
- **Not Production Ready**: Do NOT use this build in production environments
- **Testing Required**: Features may fail or behave unexpectedly

## Reporting Issues

Found a bug? Help us improve! Please report issues on our [Issues Page](../../issues) with:

📝 **Detailed Information**:
- Steps to reproduce the issue
- Expected vs. actual behavior
- Screenshots or error logs (if applicable)
- Your environment (OS, ViewTouch version, build type)
- Relevant code snippets or stack traces

📋 **Issue Template**:
```
**Describe the bug:**
[Clear description of the issue]

**Steps to reproduce:**
1. [First step]
2. [Second step]
3. [What happens]

**Expected behavior:**
[What should happen]

**Environment:**
- OS: [e.g., Ubuntu 20.04]
- ViewTouch Version: [e.g., dev branch, commit hash]
- Build Type: [Debug/Release]

**Additional context:**
[Any other relevant information]
```

## Getting Started

### Build and Test

```bash
# Clone the repository
git clone https://github.com/ViewTouch/viewtouch.git
cd viewtouch
git checkout dev

# Build (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure -j$(nproc)

# Build (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Contribute

1. **Create a feature branch** from `dev`
2. **Make your changes** and commit with clear messages
3. **Test thoroughly** — build, run tests, smoke test the UI
4. **Submit a pull request** with detailed description
5. **Respond to feedback** and iterate

## Branch Strategy

- **`master`**: Official ViewTouch repository mirror (stable, upstream-aligned)
- **`dev`**: Development and testing branch (active work, may be unstable)

## Resources

- 📚 [Documentation](./docs/)
- 🐛 [Report Issues](../../issues)
- 💬 [Discussions](../../discussions)
- 📖 [Changelog](./docs/changelog.md)

---

**Happy coding! Together, we're building the future of ViewTouch.** 🚀
