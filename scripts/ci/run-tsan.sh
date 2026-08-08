#!/bin/bash
# Phase C Quality: TSan Build and Test Script

set -e

echo "=== Building with ThreadSanitizer (TSan) ==="

# Clean previous build
rm -rf build-tsan

# Configure with TSan
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNETP2_ENABLE_TSAN=ON

# Build
cmake --build build-tsan -j

echo ""
echo "=== Running race condition tests with TSan ==="
./build-tsan/tests/netp2_config_race_test

echo ""
echo "=== Running all tests with TSan ==="
ctest --test-dir build-tsan --output-on-failure

echo ""
echo "=== TSan validation complete ==="
echo "Check for data races and thread safety issues above"
