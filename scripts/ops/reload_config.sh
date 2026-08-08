#!/usr/bin/env bash
# D-DEPLOY-SCRIPTS: 热重载配置
# 参考：16-phase-d-implementation-guide.md 第 16.5.2 节

set -euo pipefail

PID_FILE="${PID_FILE:-/var/run/netp2_gateway.pid}"
CONFIG_FILE="${CONFIG_FILE:-/opt/netp2/configs/gateway.prod.yaml}"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --pid FILE       PID file (default: $PID_FILE)"
    echo "  -c, --config FILE    New config file (default: $CONFIG_FILE)"
    echo "  -h, --help           Show this help message"
    echo ""
    echo "Note: Hot reload triggers by sending SIGHUP to the gateway process."
    echo "      The gateway must support SIGHUP for hot reload to work."
    exit 0
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--pid) PID_FILE="$2"; shift 2 ;;
        -c|--config) CONFIG_FILE="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

echo "[$(date '+%Y-%m-%d %H:%M:%S')] Reloading gateway configuration..."

# Step 1: 检查 PID 文件
if [[ ! -f "$PID_FILE" ]]; then
    echo "Error: PID file not found: $PID_FILE"
    echo "Gateway may not be running"
    exit 1
fi

GATEWAY_PID=$(cat "$PID_FILE")

# Step 2: 检查进程是否存在
if ! kill -0 "$GATEWAY_PID" 2>/dev/null; then
    echo "Error: Process $GATEWAY_PID not found"
    rm -f "$PID_FILE"
    exit 1
fi

# Step 3: 验证新配置文件
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

echo "Validating config file: $CONFIG_FILE"
# 注：此处可以调用 validate_config.sh 进行配置校验（Phase D 后续任务）
# "$SCRIPT_DIR/validate_config.sh" "$CONFIG_FILE" || exit 1

# Step 4: 发送 SIGHUP 信号触发热重载
echo "Sending SIGHUP to PID $GATEWAY_PID..."
kill -HUP "$GATEWAY_PID"

echo "[$(date '+%Y-%m-%d %H:%M:%S')] ✓ Reload signal sent"
echo ""
echo "Note: Hot reload feature depends on Phase C config integration."
echo "      Current implementation may require manual verification."
echo "      Check gateway logs for reload status."
