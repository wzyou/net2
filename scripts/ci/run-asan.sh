#!/bin/bash
# Phase C Quality: ASan Build and Test Script

set -e

echo "=== Building with AddressSanitizer (ASan/LSan) ==="

# Clean previous build
rm -rf build-asan

# Configure with ASan
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNETP2_ENABLE_ASAN=ON

# Build
cmake --build build-asan -j

echo ""
echo "=== Running tests with ASan ==="
ctest --test-dir build-asan --output-on-failure

echo ""
echo "=== ASan validation complete ==="
echo "Check for memory leaks and use-after-free errors above"
