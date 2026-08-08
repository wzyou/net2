#!/usr/bin/env bash
# D-DEPLOY-SCRIPTS: 启动网关
# 参考：16-phase-d-implementation-guide.md 第 16.5.2 节

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${INSTALL_DIR:-/opt/netp2}"
CONFIG_FILE="${CONFIG_FILE:-$INSTALL_DIR/configs/gateway.prod.yaml}"
PID_FILE="${PID_FILE:-/var/run/netp2_gateway.pid}"
LOG_FILE="${LOG_FILE:-/var/log/netp2_gateway.log}"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -c, --config FILE    Config file (default: $CONFIG_FILE)"
    echo "  -p, --pid FILE       PID file (default: $PID_FILE)"
    echo "  -l, --log FILE       Log file (default: $LOG_FILE)"
    echo "  -h, --help           Show this help message"
    exit 0
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--config) CONFIG_FILE="$2"; shift 2 ;;
        -p|--pid) PID_FILE="$2"; shift 2 ;;
        -l|--log) LOG_FILE="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Starting netp2 gateway..."

# Step 1: 检查是否已运行
if [[ -f "$PID_FILE" ]]; then
    OLD_PID=$(cat "$PID_FILE")
    if kill -0 "$OLD_PID" 2>/dev/null; then
        echo "Error: Gateway is already running (PID: $OLD_PID)"
        exit 1
    else
        echo "Warning: Stale PID file found, removing..."
        rm -f "$PID_FILE"
    fi
fi

# Step 2: 检查二进制文件
GATEWAY_BIN="$INSTALL_DIR/bin/netp2_gateway"
if [[ ! -x "$GATEWAY_BIN" ]]; then
    echo "Error: Gateway binary not found or not executable: $GATEWAY_BIN"
    exit 1
fi

# Step 3: 检查配置文件
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

# Step 4: 启动网关
echo "Starting gateway with config: $CONFIG_FILE"
mkdir -p "$(dirname "$LOG_FILE")"
mkdir -p "$(dirname "$PID_FILE")"

"$GATEWAY_BIN" --config "$CONFIG_FILE" >> "$LOG_FILE" 2>&1 &
GATEWAY_PID=$!

echo $GATEWAY_PID > "$PID_FILE"
echo "[$(date '+%Y-%m-%d %H:%M:%S')] Gateway started (PID: $GATEWAY_PID)"

# Step 5: 等待健康检查
echo "Waiting for gateway to be ready..."
for i in {1..30}; do
    if "$SCRIPT_DIR/healthcheck.sh" "http://localhost:8080/health" 2>/dev/null; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Gateway is ready"
        echo ""
        echo "Gateway PID: $GATEWAY_PID"
        echo "Config file: $CONFIG_FILE"
        echo "Log file: $LOG_FILE"
        exit 0
    fi
    sleep 1
done

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✗ Gateway failed to become ready within 30 seconds"
echo "Check logs at: $LOG_FILE"
exit 1
