#!/usr/bin/env bash
# D-BENCHMARK-CI: 性能基准门禁脚本
# 参考：16-phase-d-implementation-guide.md 第 16.4.2 节

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-release}"
BENCHMARK_DIR="$PROJECT_ROOT/benchmarks/wrk"
BASELINE_FILE="${BASELINE_FILE:-$BENCHMARK_DIR/baseline_relwithdebinfo_30s.json}"
CURRENT_OUTPUT="$BENCHMARK_DIR/ci_current_run.json"

echo "=========================================="
echo "  Performance Benchmark Gate"
echo "=========================================="
echo ""

# Step 1: 检查前置条件
echo "[Step 1] Checking prerequisites..."
if [[ ! -f "$BUILD_DIR/src/netp2_gateway" ]]; then
    echo "Error: Gateway binary not found at $BUILD_DIR/src/netp2_gateway"
    echo "Please build with: cmake -S . -B build-release -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build-release -j"
    exit 1
fi

if ! command -v wrk &> /dev/null; then
    echo "Error: wrk not found. Please install wrk first."
    exit 1
fi

if [[ ! -f "$BASELINE_FILE" ]]; then
    echo "Error: Baseline file not found at $BASELINE_FILE"
    echo "Please run local baseline first: cd benchmarks/wrk && ./run_baseline.sh"
    exit 1
fi

echo "✓ Prerequisites check passed"
echo ""

# Step 2: 启动网关
echo "[Step 2] Starting gateway..."
"$BUILD_DIR/src/netp2_gateway" --config "$PROJECT_ROOT/configs/gateway.dev.yaml" &
GATEWAY_PID=$!
echo "Gateway PID: $GATEWAY_PID"

# 等待网关就绪
echo "Waiting for gateway to be ready..."
for i in {1..20}; do
    if curl -sf http://localhost:18080/metrics > /dev/null 2>&1; then
        echo "✓ Gateway ready (attempt $i)"
        break
    fi
    if [[ $i -eq 20 ]]; then
        echo "✗ Gateway failed to start within 20 seconds"
        kill -TERM $GATEWAY_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done
echo ""

# Step 3: 执行基准测试
echo "[Step 3] Running benchmark..."
cd "$BENCHMARK_DIR"
if ! ./run_baseline.sh --duration 30s --threads 4 --connections 100 --output "$CURRENT_OUTPUT"; then
    echo "✗ Benchmark execution failed"
    kill -TERM $GATEWAY_PID 2>/dev/null || true
    exit 1
fi
cd "$PROJECT_ROOT"
echo ""

# Step 4: 停止网关
echo "[Step 4] Stopping gateway..."
kill -TERM $GATEWAY_PID 2>/dev/null || true
wait $GATEWAY_PID 2>/dev/null || true
echo "✓ Gateway stopped"
echo ""

# Step 5: 对比基线
echo "[Step 5] Comparing with baseline..."
if ! python3 "$SCRIPT_DIR/compare_baseline.py" "$BASELINE_FILE" "$CURRENT_OUTPUT"; then
    echo ""
    echo "=========================================="
    echo "  ✗ PERFORMANCE REGRESSION DETECTED"
    echo "=========================================="
    exit 1
fi

echo ""
echo "=========================================="
echo "  ✓ BENCHMARK GATE PASSED"
echo "=========================================="
exit 0
