# 11. 项目脚手架架构评估与落地方案

## 11.1 评估结论（Architect）
当前仓库已经具备“架构真理源”基础（宪法、分册、ADR、Agent 规则），但缺少可执行工程脚手架。

结论：有条件通过。
- 优势：架构边界清晰，AI 约束已建立，文档结构可检索。
- 主要缺口：构建系统、源码目录、测试与基准目录、CI 门禁、配置样例与运维脚本尚未落地。
- 风险：若直接进入编码阶段，容易发生实现分歧、质量门禁缺失、性能回归不可观测。

## 11.2 目标脚手架（建议目录）
```text
netp2/
├── .github/
│   ├── copilot-instructions.md
│   └── workflows/
│       ├── ci.yml
│       ├── sanitizer.yml
│       └── benchmark.yml
├── .vscode/
│   └── settings.json
├── agents_rules/
├── cmake/
│   ├── toolchains/
│   ├── sanitizers.cmake
│   └── warnings.cmake
├── configs/
│   ├── gateway.dev.yaml
│   ├── gateway.prod.yaml
│   └── routes.sample.yaml
├── docs/
│   ├── architecture/
│   └── adr/
├── include/netp2/
│   ├── core/
│   ├── runtime/
│   ├── routing/
│   ├── upstream/
│   ├── resilience/
│   ├── observability/
│   └── config/
├── src/
│   ├── app/
│   ├── runtime/
│   ├── protocol/
│   ├── routing/
│   ├── upstream/
│   ├── resilience/
│   ├── observability/
│   └── config/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── race/
│   └── e2e/
├── benchmarks/
│   ├── wrk/
│   └── ghz/
├── scripts/
│   ├── dev/
│   ├── ci/
│   └── ops/
├── third_party/
├── CMakeLists.txt
└── README.md
```

## 11.3 模块边界与线程亲和约束
- `runtime/`：Worker 生命周期、io_context、acceptor、CPU 亲和绑定。
- `protocol/`：HTTP/1.1 与 HTTP/2 解析、RequestContext 统一抽象。
- `routing/`：Host + Path(Trie) + IP 匹配，路由只读快照。
- `upstream/`：thread_local Keep-Alive 池，建连与超时控制。
- `resilience/`：限流、断路器、主动探活。
- `config/`：配置加载、编译、RCU 发布。
- `observability/`：thread_local metrics、trace 传播、聚合导出。

约束：
- 热路径禁止跨线程共享可变状态。
- 所有跨协程持有的共享快照必须有可证明生命周期。
- 所有原子发布/读取要显式说明 memory order 与 happens-before。

## 11.4 构建矩阵（客观真理）
建议最少维护 4 套构建配置：
1. `Debug`：开发联调。
2. `RelWithDebInfo`：性能诊断。
3. `AsanLsan`：内存安全（`-fsanitize=address`）。
4. `Tsan`：竞态探测（`-fsanitize=thread`，与 ASan 分离）。

统一告警基线：
- `-Wall -Wextra -Wpedantic`
- 建议逐步升级为 `-Werror`（先在 CI 层对核心目录启用）。

## 11.5 测试与性能门禁
- 单元测试：核心数据结构、状态机、限流算法。
- 集成测试：端到端代理链路、异常与超时路径。
- 竞态测试：在 `race/` 目录复现高并发共享边界。
- 性能基线：
1. HTTP 基准 `wrk`
2. gRPC 基准 `ghz`
3. 指标最少包含吞吐、P50/P90/P99、错误率、CPU、RSS。

建议门禁阈值：
- 吞吐下降 > 5% 失败。
- P99 上升 > 10% 失败。
- 错误率高于基线 + 0.2% 失败。

## 11.6 分阶段落地计划
### Phase A（Day 0-2）
- 建立 CMake 根工程与模块子目录。
- 建立测试骨架（GoogleTest 或同级方案）。
- 建立 CI 基础流水线（编译 + 单测）。
- 打通 Runtime Spine（单核）：`io_context` + `acceptor` + `co_spawn` 最小事件循环。
- 明确 Proactor 最小验收：I/O 路径为 awaitable 协程风格且无阻塞调用。

Phase A 执行状态（2026-08-08）：已完成
- 构建组织：根 `CMakeLists.txt` 已切换为 `src/`、`tests/` 子目录化管理。
- Runtime Spine：已落地 `RuntimeSpine`（单核 `io_context` + `acceptor` + `co_spawn` + awaitable I/O）。
- 应用入口：`src/app/main.cpp` 已接入 Phase A 运行主循环与信号优雅停止。
- 测试验收：新增 `tests/integration/runtime_spine_integration_test.cpp`，验证最小 accept->read->write 链路。
- CI 对齐：工作流已安装 Boost 依赖，保持编译与测试可执行。

Phase A 本地验收命令：
1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
2. `cmake --build build`
3. `ctest --test-dir build --output-on-failure`

### Phase B（Day 3-7）
- 完成 Thread-Per-Core 落地：多 Worker、每核独立 `io_context`、同端口 `SO_REUSEPORT` 接入。
- 打通同核闭环最小链路：accept -> parse -> route -> mock upstream -> response。
- 完成 executor 亲和性约束：请求主链路不得隐式跨 executor 切换。
- 引入 thread_local metrics 与基础 Prometheus 导出。

Phase A-B 里程碑约束：
- `12.1 全局运行模型` MUST 在 Phase B 结束前达成并验收。
- `12.2 协程与执行语义` MUST 在 Phase B 结束前达成并验收。

### Phase C（Week 2）
- 引入限流、断路器、连接池、RCU 热重载。
- 建立 sanitizer 与 race 任务。

### Phase D（Week 3+）
- 引入 wrk/ghz 基准与性能回归门禁。
- 补齐部署脚本、配置模板、运维手册。

## 11.7 进入编码前的 DoD（Definition of Done）
进入大规模功能开发前必须满足：
1. 工程可一键构建与运行（本地 + CI）。
2. 四套构建配置可独立执行。
3. Proactor + Thread-Per-Core 主链路已达标（多 Worker、独立 `io_context`、`SO_REUSEPORT`、协程 awaitable 风格）。
4. 基础集成测试与健康检查通过。
5. 基线性能报告可生成且可比对。
6. 文档与实现引用链完整（README -> 架构分册 -> ADR）。
