#!/usr/bin/env bash
# D-CONFIG-TEMPLATES: 配置校验脚本
# 参考：16-phase-d-implementation-guide.md 第 16.6.4 节

set -euo pipefail

CONFIG_FILE="${1:-}"

usage() {
    echo "Usage: $0 <config.yaml>"
    echo ""
    echo "Validate netp2 gateway configuration file."
    echo ""
    echo "Checks:"
    echo "  - YAML format validity"
    echo "  - Required fields presence"
    echo "  - Value range validity"
    echo "  - Referenced file paths existence"
    exit 0
}

if [[ -z "$CONFIG_FILE" ]] || [[ "$CONFIG_FILE" == "-h" ]] || [[ "$CONFIG_FILE" == "--help" ]]; then
    usage
fi

echo "Validating config file: $CONFIG_FILE"
echo ""

# Step 1: 检查文件存在
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "✗ Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

# Step 2: 验证 YAML 格式
echo "[1/4] Checking YAML syntax..."
if ! python3 -c "import yaml, sys; yaml.safe_load(open(sys.argv[1]))" "$CONFIG_FILE" 2>/dev/null; then
    echo "✗ Error: Invalid YAML syntax"
    exit 1
fi
echo "✓ YAML syntax valid"

# Step 3: 验证必填字段
echo "[2/4] Checking required fields..."
REQUIRED_FIELDS=(
    "server.listen"
    "server.workers"
    "timeouts.connect_ms"
    "timeouts.io_idle_ms"
)

for field in "${REQUIRED_FIELDS[@]}"; do
    if ! python3 -c "
import yaml, sys
cfg = yaml.safe_load(open('$CONFIG_FILE'))
keys = '$field'.split('.')
try:
    val = cfg
    for k in keys:
        val = val[k]
    print(f'✓ {\"$field\"}: {val}')
except (KeyError, TypeError):
    print(f'✗ Missing required field: {\"$field\"}')
    sys.exit(1)
" 2>/dev/null; then
        exit 1
    fi
done

# Step 4: 验证数值范围
echo "[3/4] Checking value ranges..."

# 检查 workers >= 0
WORKERS=$(python3 -c "import yaml; print(yaml.safe_load(open('$CONFIG_FILE'))['server']['workers'])" 2>/dev/null || echo "-1")
if [[ "$WORKERS" -lt 0 ]]; then
    echo "✗ Error: server.workers must be >= 0, got: $WORKERS"
    exit 1
fi
echo "✓ server.workers: $WORKERS"

# 检查超时值 > 0
CONNECT_TIMEOUT=$(python3 -c "import yaml; print(yaml.safe_load(open('$CONFIG_FILE'))['timeouts']['connect_ms'])" 2>/dev/null || echo "0")
if [[ "$CONNECT_TIMEOUT" -le 0 ]]; then
    echo "✗ Error: timeouts.connect_ms must be > 0, got: $CONNECT_TIMEOUT"
    exit 1
fi
echo "✓ timeouts.connect_ms: $CONNECT_TIMEOUT"

# Step 5: 验证引用的文件路径
echo "[4/4] Checking referenced file paths..."
ROUTES_FILE=$(python3 -c "
import yaml
cfg = yaml.safe_load(open('$CONFIG_FILE'))
routes = cfg.get('routing', {}).get('rules_file')
print(routes if routes else '')
" 2>/dev/null || echo "")

if [[ -n "$ROUTES_FILE" ]]; then
    CONFIG_DIR="$(dirname "$CONFIG_FILE")"
    ROUTES_PATH="$CONFIG_DIR/$ROUTES_FILE"
    
    if [[ -f "$ROUTES_PATH" ]]; then
        echo "✓ routing.rules_file exists: $ROUTES_PATH"
    else
        echo "⚠️  Warning: routing.rules_file not found: $ROUTES_PATH"
        echo "   (This is acceptable if routes are not configured yet)"
    fi
else
    echo "✓ No routing.rules_file specified (will use defaults)"
fi

echo ""
echo "=========================================="
echo "  ✓ Configuration Valid"
echo "=========================================="
exit 0
