#!/usr/bin/env bash
# D-DEPLOY-SCRIPTS: 停止网关
# 参考：16-phase-d-implementation-guide.md 第 16.5.2 节

set -euo pipefail

PID_FILE="${PID_FILE:-/var/run/netp2_gateway.pid}"
TIMEOUT="${TIMEOUT:-30}"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --pid FILE       PID file (default: $PID_FILE)"
    echo "  -t, --timeout SEC    Graceful shutdown timeout (default: 30s)"
    echo "  -h, --help           Show this help message"
    exit 0
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--pid) PID_FILE="$2"; shift 2 ;;
        -t|--timeout) TIMEOUT="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Stopping netp2 gateway..."

# Step 1: 检查 PID 文件
if [[ ! -f "$PID_FILE" ]]; then
    echo "Warning: PID file not found: $PID_FILE"
    echo "Gateway may not be running"
    exit 0
fi

GATEWAY_PID=$(cat "$PID_FILE")

# Step 2: 检查进程是否存在
if ! kill -0 "$GATEWAY_PID" 2>/dev/null; then
    echo "Warning: Process $GATEWAY_PID not found"
    rm -f "$PID_FILE"
    exit 0
fi

# Step 3: 发送 SIGTERM 信号（优雅停止）
echo "Sending SIGTERM to PID $GATEWAY_PID..."
kill -TERM "$GATEWAY_PID"

# Step 4: 等待进程退出
echo "Waiting for graceful shutdown (timeout: ${TIMEOUT}s)..."
for ((i=0; i<TIMEOUT; i++)); do
    if ! kill -0 "$GATEWAY_PID" 2>/dev/null; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Gateway stopped gracefully"
        rm -f "$PID_FILE"
        exit 0
    fi
    sleep 1
done

# Step 5: 强制停止
echo "Warning: Graceful shutdown timeout, sending SIGKILL..."
kill -KILL "$GATEWAY_PID" 2>/dev/null || true
rm -f "$PID_FILE"
echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Gateway stopped (forced)"
exit 0
