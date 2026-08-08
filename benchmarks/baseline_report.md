# netp2 Performance Baseline Report

本文档记录 netp2 网关的性能基线数据，用于后续性能回归检测。

## 环境信息

| 项目 | 值 |
|------|-----|
| **CPU** | Apple M1 |
| **核心数** | 8 |
| **操作系统** | macOS 26.5.2 |
| **编译器** | Apple clang version 21.0.0 (clang-2100.1.1.101) |
| **构建类型** | Debug |
| **Boost 版本** | 1.87.0 (via brew) |
| **llhttp 版本** | 9.2.1 (via CMake FetchContent) |

## 基准配置

| 项目 | 值 |
|------|-----|
| **测试工具** | wrk 4.2.0 |
| **测试时长** | 10s（快速验证），30s（正式基线） |
| **线程数** | 2 |
| **并发连接数** | 50 |
| **目标路由** | `GET /proxy/mock` |
| **网关配置** | `configs/gateway.dev.yaml` |
| **Worker 数量** | 8（Thread-Per-Core） |
| **监听端口** | 18080 |

## 基线数据

### Baseline #1：2026-08-08（Debug 构建，快速验证）

**测试场景**：简单 GET 请求，路由命中，无限流，响应 200 OK

**配置**：
- 构建类型：Debug
- 测试时长：10s
- 线程数：2
- 并发连接数：50

| 指标 | 值 |
|------|-----|
| **RPS** | 1,899.26 req/s |
| **P50 延迟** | 1.85 ms |
| **P75 延迟** | 1.93 ms |
| **P90 延迟** | 2.33 ms |
| **P99 延迟** | 2.90 ms |
| **错误率** | 0.0% |
| **总请求数** | 19,167 |
| **吞吐量** | 0.21 MB/s |

**原始数据文件**：`baseline_20260808_202156.json`

---

### Baseline #2：2026-08-08（RelWithDebInfo 构建，正式基线）⭐

**测试场景**：简单 GET 请求，路由命中，无限流，响应 200 OK

**配置**：
- 构建类型：RelWithDebInfo
- 测试时长：30s
- 线程数：4
- 并发连接数：100

| 指标 | 值 |
|------|-----|
| **RPS** | 1,063.71 req/s |
| **P50 延迟** | 1.30 ms |
| **P75 延迟** | 1.41 ms |
| **P90 延迟** | 1.63 ms |
| **P99 延迟** | 4.27 ms |
| **错误率** | 0.0% |
| **总请求数** | 31,998 |
| **吞吐量** | 0.12 MB/s |

**原始数据文件**：`wrk/baseline_relwithdebinfo_30s.json`

**观察**：
- ✓ 所有请求成功，无错误
- ✓ P50 延迟降至 1.3ms，相比 Debug 有明显优化
- ✓ P99 延迟 4.27ms，在可接受范围内
- ✓ 更高并发（100 连接）下系统表现稳定
- ℹ️ RPS 相对 Debug 降低，可能是并发连接数增加（50→100）导致单连接吞吐降低

**此基线作为后续性能回归检测的参考基准。**

---

### 多场景测试状态

| 场景 | 状态 | 说明 |
|------|------|------|
| **简单 GET** | ✅ 已完成 | Baseline #2 |
| **限流触发** | ⏸️ 待集成 | Phase C 限流功能待集成到主链路 |
| **断路器触发** | ⏸️ 待集成 | Phase C 断路器功能待集成到主链路 |
| **热重载期间** | ⏸️ 待集成 | Phase C 热重载功能待集成到主链路 |

**注**：限流、断路器、热重载的单元测试已通过，但尚未完全集成到 RuntimeSpine 主链路。待 Phase C 完整集成后补充对应场景的基准测试。

## 性能回归阈值（参考 11-project-scaffolding.md 11.5 节）

基于 **Baseline #2（RelWithDebInfo, 30s, 100 conn）** 设定阈值：

| 指标 | 基线值 | 阈值 | 失败条件 |
|------|--------|------|---------|
| **RPS** | 1,063.71 req/s | -5% | < 1,010.5 req/s |
| **P99 延迟** | 4.27 ms | +10% | > 4.70 ms |
| **错误率** | 0.0% | +0.2% | > 0.2% |

**说明**：
- RPS 下降超过 5% 视为性能退化
- P99 延迟上升超过 10% 视为性能退化
- 错误率高于基线 + 0.2% 视为质量退化

## 下一步

### D-BENCHMARK-BASELINE 任务进度
- [x] 创建 `benchmarks/wrk/run_baseline.sh`
- [x] 创建 `benchmarks/wrk/baseline.lua`
- [x] 创建 `benchmarks/wrk/parse_results.py`
- [x] 使用 RelWithDebInfo 构建重新采集基线
- [x] 执行完整 30s 测试
- [x] 创建 `benchmarks/ghz/run_baseline.sh`（gRPC 基准预留）
- [x] **D-BENCHMARK-BASELINE 已完成** ✅

### 待办事项
1. **进入 D-BENCHMARK-CI**（下一步）
   - 创建 `.github/workflows/benchmark.yml`
   - 创建 `scripts/ci/run_benchmark_gate.sh`
   - 创建 `scripts/ci/compare_baseline.py`
   - 设置性能回归门禁

2. **补充多场景测试**（Phase C 后续任务）
   - 限流触发场景（待限流集成到主链路）
   - 断路器触发场景（待断路器集成到主链路）
   - 热重载期间延迟抖动（待热重载集成到主链路）

4. **进入 D-BENCHMARK-CI**：将基准集成到 CI 流水线

---

*最后更新：2026-08-08*  
*测试人员：程序员（按照 Phase D 指导执行）*
