# Build Instructions for macOS
#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

# Build all executables in the project
cmake -S . -B build-macos -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++

# Can adjust amount of workers
cmake --build build-macos --parallel 2

# Run executable
# ./build-macos/media_demo
