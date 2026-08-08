# CI Scripts

本目录包含 CI/CD 流程使用的脚本。

## 依赖管理

### llhttp 依赖

**llhttp** (HTTP/1.1 解析库) 通过 CMake FetchContent 自动获取，无需手动安装。

CMakeLists.txt 配置：
```cmake
FetchContent_Declare(
    llhttp
    URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.4.3.tar.gz"
)
FetchContent_MakeAvailable(llhttp)
```

CMake 会在配置阶段自动：
1. 下载 llhttp v9.4.3 源码
2. 构建静态库 `libllhttp.a`
3. 使目标 `llhttp_static` 可用于链接

**无需在 CI 或本地环境中预先安装 llhttp**。

## 质量检查

### run-asan.sh
使用 AddressSanitizer 构建并测试，检测内存泄漏和 use-after-free。

```bash
scripts/ci/run-asan.sh
```

### run-tsan.sh
使用 ThreadSanitizer 构建并测试，检测数据竞争和线程安全问题。

```bash
scripts/ci/run-tsan.sh
```

## 构建测试

### build_and_test.sh
基础构建和测试脚本（占位符）。

## GitHub Actions Workflows

CI 配置位于 `.github/workflows/`:

- **ci.yml**: 主 CI 流程（每次 push/PR 触发）
  - 安装依赖（CMake, Ninja, GCC, Boost）
  - 配置项目（CMake 自动获取 llhttp）
  - 构建项目
  - 运行所有测试

- **sanitizer.yml**: Sanitizer 检查（PR 或手动触发）
  - ASan/LSan 作业：内存安全检查
  - TSan 作业：线程安全检查

- **benchmark.yml**: 性能基准测试（手动触发）
  - 占位符，待实现

## 本地开发

### 依赖安装

**必需依赖**：
```bash
# macOS
brew install cmake ninja boost

# Ubuntu/Debian
sudo apt-get install cmake ninja-build g++ libboost-system-dev
```

**llhttp**: 通过 CMake FetchContent 自动获取，无需手动安装。

### 构建项目

```bash
# 配置（首次会下载 llhttp 源码）
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 编译
cmake --build build -j$(nproc)

# 测试
cd build && ctest
```

## 故障排查

### FetchContent 下载失败
如果 CMake 无法下载 llhttp：

```bash
# 检查网络连接
curl -L https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.4.3.tar.gz -o /tmp/test.tar.gz

# 清理并重试
rm -rf build/_deps/llhttp-*
cmake -S . -B build
```

### Sanitizer 报错
参见 [docs/architecture/15-phase-c-quality-validation.md](../../docs/architecture/15-phase-c-quality-validation.md)

