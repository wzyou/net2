#!/usr/bin/env bash
# D-DEPLOY-SCRIPTS: 构建 Release 版本
# 参考：16-phase-d-implementation-guide.md 第 16.5.2 节

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
BUILD_DIR="$PROJECT_ROOT/build-release"
PACKAGE_DIR="$PROJECT_ROOT/package"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -t, --type TYPE      Build type (RelWithDebInfo|Release, default: RelWithDebInfo)"
    echo "  -j, --jobs N         Parallel build jobs (default: auto)"
    echo "  -o, --output DIR     Package output directory (default: package/)"
    echo "  -h, --help           Show this help message"
    exit 0
}

# 解析参数
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type) BUILD_TYPE="$2"; shift 2 ;;
        -j|--jobs) JOBS="$2"; shift 2 ;;
        -o|--output) PACKAGE_DIR="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

echo "=========================================="
echo "  Building netp2 Gateway"
echo "=========================================="
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $JOBS"
echo "Package output: $PACKAGE_DIR"
echo ""

# Step 1: 清理旧构建
if [[ -d "$BUILD_DIR" ]]; then
    echo "[Step 1] Cleaning old build..."
    rm -rf "$BUILD_DIR"
fi

# Step 2: 配置 CMake
echo "[Step 2] Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="/opt/netp2"

# Step 3: 构建
echo "[Step 3] Building..."
cmake --build "$BUILD_DIR" -j "$JOBS"

# Step 4: 运行测试
echo "[Step 4] Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

# Step 5: 打包
echo "[Step 5] Packaging..."
mkdir -p "$PACKAGE_DIR/bin"
mkdir -p "$PACKAGE_DIR/configs"
mkdir -p "$PACKAGE_DIR/scripts"

cp "$BUILD_DIR/src/netp2_gateway" "$PACKAGE_DIR/bin/"
cp -r "$PROJECT_ROOT/configs/"*.yaml "$PACKAGE_DIR/configs/" 2>/dev/null || true
cp "$PROJECT_ROOT/scripts/ops/"*.sh "$PACKAGE_DIR/scripts/"

# 创建版本信息
cat > "$PACKAGE_DIR/VERSION" <<EOF
BUILD_TYPE=$BUILD_TYPE
BUILD_DATE=$(date -u +"%Y-%m-%d %H:%M:%S UTC")
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git branch --show-current 2>/dev/null || echo "unknown")
EOF

echo ""
echo "=========================================="
echo "  ✓ Build Complete"
echo "=========================================="
echo "Package location: $PACKAGE_DIR"
echo "Binary: $PACKAGE_DIR/bin/netp2_gateway"
echo ""
ls -lh "$PACKAGE_DIR/bin/netp2_gateway"
