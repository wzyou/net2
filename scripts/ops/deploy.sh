#!/usr/bin/env bash
# D-DEPLOY-SCRIPTS: 部署脚本
# 参考：16-phase-d-implementation-guide.md 第 16.5.2 节

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGE_DIR="${PACKAGE_DIR:-$PROJECT_ROOT/package}"
INSTALL_DIR="${INSTALL_DIR:-/opt/netp2}"
BACKUP_DIR="/opt/netp2.backup.$(date +%Y%m%d_%H%M%S)"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -s, --source DIR     Package directory (default: $PACKAGE_DIR)"
    echo "  -d, --dest DIR       Install directory (default: $INSTALL_DIR)"
    echo "  -b, --backup         Backup existing installation"
    echo "  -h, --help           Show this help message"
    exit 0
}

# 解析参数
DO_BACKUP=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--source) PACKAGE_DIR="$2"; shift 2 ;;
        -d|--dest) INSTALL_DIR="$2"; shift 2 ;;
        -b|--backup) DO_BACKUP=true; shift ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

echo "=========================================="
echo "  Deploying netp2 Gateway"
echo "=========================================="
echo "Source: $PACKAGE_DIR"
echo "Destination: $INSTALL_DIR"
echo "Backup: $DO_BACKUP"
echo ""

# Step 1: 检查包目录
if [[ ! -d "$PACKAGE_DIR" ]]; then
    echo "Error: Package directory not found: $PACKAGE_DIR"
    echo "Please run: scripts/ops/build_release.sh"
    exit 1
fi

if [[ ! -f "$PACKAGE_DIR/bin/netp2_gateway" ]]; then
    echo "Error: Gateway binary not found in package"
    exit 1
fi

# Step 2: 备份现有安装
if [[ -d "$INSTALL_DIR" ]] && [[ "$DO_BACKUP" == "true" ]]; then
    echo "[Step 2] Backing up existing installation..."
    cp -r "$INSTALL_DIR" "$BACKUP_DIR"
    echo "Backup created: $BACKUP_DIR"
fi

# Step 3: 创建目标目录
echo "[Step 3] Creating installation directories..."
mkdir -p "$INSTALL_DIR"/{bin,configs,scripts,logs}

# Step 4: 部署文件
echo "[Step 4] Deploying files..."
cp -v "$PACKAGE_DIR/bin/netp2_gateway" "$INSTALL_DIR/bin/"
chmod +x "$INSTALL_DIR/bin/netp2_gateway"

if [[ -d "$PACKAGE_DIR/configs" ]]; then
    cp -rv "$PACKAGE_DIR/configs/"*.yaml "$INSTALL_DIR/configs/" 2>/dev/null || true
fi

if [[ -d "$PACKAGE_DIR/scripts" ]]; then
    cp -rv "$PACKAGE_DIR/scripts/"*.sh "$INSTALL_DIR/scripts/" 2>/dev/null || true
    chmod +x "$INSTALL_DIR/scripts/"*.sh 2>/dev/null || true
fi

if [[ -f "$PACKAGE_DIR/VERSION" ]]; then
    cp -v "$PACKAGE_DIR/VERSION" "$INSTALL_DIR/"
fi

# Step 5: 验证部署
echo "[Step 5] Verifying deployment..."
if [[ ! -x "$INSTALL_DIR/bin/netp2_gateway" ]]; then
    echo "Error: Deployment verification failed"
    exit 1
fi

echo ""
echo "=========================================="
echo "  ✓ Deployment Complete"
echo "=========================================="
echo "Installation directory: $INSTALL_DIR"
echo "Binary: $INSTALL_DIR/bin/netp2_gateway"
echo ""
echo "Next steps:"
echo "  1. Review config: $INSTALL_DIR/configs/gateway.prod.yaml"
echo "  2. Start gateway: $INSTALL_DIR/scripts/start.sh"
echo "  3. Check health: $INSTALL_DIR/scripts/healthcheck.sh"
