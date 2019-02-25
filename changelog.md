# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)

## [Unreleased]
### Added

### Changed

### Removed

### Fixed
- segfault because of `input_id` narrowing conversion
- unhandled exception when ConfFile doesn't exist
- don't try to render reports that failed to load


## [v19.02.1] - 2019-02-16
### Added
- support for building with clang toolchain

### Fixed
- armhf and arm64 builds by using exact types for read and write operations
- segfault when printing a work order when no printer is configured
- crash on shutdown preventing modifications to interface to be saved


## [v19.01.1] - 2019-01-21
First release after versioning change

