#!/bin/bash
# Simple wrapper script to check ViewTouch dependencies
# Usage: ./check_dependencies.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmake -P "${SCRIPT_DIR}/cmake/install_dependencies.cmake"

