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

Phase B 执行状态（Step 1，2026-08-08）：已完成
- 新增多 Worker 运行时：`GatewayRuntime`，每个 Worker 拥有独立 `io_context` 与 `RuntimeSpine`。
- 接入同端口监听：`RuntimeSpine` 支持 `SO_REUSEPORT` 开关并在主程序启用。
- 主程序已切换为按 CPU 核数启动 Worker，保留信号驱动的优雅停止流程。
- 新增集成测试：`netp2_worker_runtime_integration_test`，验证多 Worker 接入后请求可达。

Phase B 执行状态（Step 2，2026-08-08）：已完成
- 在 `RuntimeSpine` 的 accept 与连接协程路径新增 executor 亲和性检查点。
- 新增违规计数器（per-spine 与 runtime 聚合），用于验证请求主链路无隐式跨 executor 切换。
- 在 `netp2_worker_runtime_integration_test` 增加断言：亲和性违规计数必须为 0。

Phase B 执行状态（Step 3，2026-08-08）：已完成
- 新增 `MetricsRegistry` 与 Worker 分片指标模型，Worker 线程启动时绑定 thread_local 指标分片。
- 在 `RuntimeSpine` 最小链路接入打点：accept、request、response、bytes in/out。
- 新增 `/metrics` 基础导出端点，按抓取时聚合输出 Prometheus 文本格式。
- 新增集成测试：`netp2_metrics_export_integration_test`，验证指标端点可用与关键指标暴露。

Phase B 执行状态（Step 4，2026-08-08）：已完成
- 在 `RuntimeSpine` 落地最小请求主链路：`accept -> parse(request-line) -> route -> mock upstream -> response`。
- 新增路由分支：`/proxy/mock` 命中 mock upstream（200），未知路径返回 404，不合法请求返回 400。
- 在主链路补充显式公平让出点（`asio::post`），避免连接协程长时间占用同核执行权。
- 新增集成测试：`netp2_pipeline_integration_test`，覆盖命中、未命中、坏请求三类路径。

Phase A-B 里程碑约束：
- `12.1 全局运行模型` MUST 在 Phase B 结束前达成并验收。
- `12.2 协程与执行语义` MUST 在 Phase B 结束前达成并验收。

### Phase C（Week 2）
执行指导见 `14-phase-c-implementation-guide.md`。Phase C 必须先关闭协议解析与 `RequestContext` 归一缺口，再推进路由、限流、upstream、热重载与质量门禁。

- C-PROTO-1：实现 HTTP/1.1 `llhttp` codec，将请求行、Header、Body 元信息归一到 `RequestContext`。
- C-PROTO-2：补齐协议校验边界，包括 Host 必填、Header 大小/数量上限、Content-Length 一致性、Chunked 解析与错误响应映射。
- C-PROTO-3：协议层接入 thread_local Buffer 与 RAII 借还模型，确保解析热路径不引入跨线程共享状态。
- C-PROTO-4：将 `RuntimeSpine` 中 Phase B 临时 request-line 解析替换为 `src/protocol/` 模块输出的 `RequestContext`。
- C-PROTO-5：接入 HTTP/2 `nghttp2` 卸载路径，并保持上层只消费统一 `RequestContext`。
- C-ROUTE-1：引入 Host + Path(Trie LPM) + Client IP 三维路由匹配，并将规则编译为只读快照。
- C-RESILIENCE-1：引入 thread_local 限流、三态断路器与可观测拒绝路径。
- C-UPSTREAM-1：引入按核心本地化的 upstream Keep-Alive 连接池，覆盖复用前健康检查与建连超时。
- C-CONFIG-1：落地 RCU 热重载流程：新快照构建 -> 按核投递 -> release/acquire 切换 -> 旧版本自然回收。
- C-QUALITY-1：建立 sanitizer 与 race 任务，覆盖协议解析、资源回收、热重载同步边界。

Phase C 里程碑约束：
- `12.3 内存与协议栈` MUST 作为 Phase C 第一优先级闭环，HTTP/1.1 主路径不得继续依赖 Phase B 临时 request-line 解析。
- `12.4 路由与限流`、`12.5 Upstream 与韧性`、`12.6 热重载与内存模型` MUST 在 Phase C 结束前具备最小可验收实现。
- 每个 C-* 任务必须在 PR 中附 “MUST 条款 -> 代码位置 -> 验证命令/测试证据”。

### Phase D（Week 3+）
执行指导见 `16-phase-d-implementation-guide.md`。Phase D 目标是建立可重复的性能基准体系与生产就绪的运维支撑。

- D-BENCHMARK-BASELINE：建立本地基准脚本与基线数据采集流程（wrk/ghz）。
- D-BENCHMARK-CI：将基准集成到 CI，建立性能回归门禁（吞吐/P99/错误率阈值）。
- D-DEPLOY-SCRIPTS：补齐构建、部署、启停、健康检查、热重载脚本。
- D-CONFIG-TEMPLATES：补齐生产级配置模板（`gateway.template.yaml`）与配置校验脚本。
- D-OPS-RUNBOOK：编写运维手册，覆盖部署、配置、可观测性、故障排查、升级回滚。
- D-VALIDATION：全流程验收与文档对齐。

Phase D 里程碑约束：
- `12.7 可观测与门禁` MUST 在 Phase D 结束前具备可执行的性能基准与回归门禁。
- `12.8 工程与治理` MUST 在 Phase D 结束前具备可重复的部署与运维流程。
- 每个 D-* 任务必须在 PR 中附 "MUST 条款 -> 实现位置 -> 验收证据"。

## 11.7 进入编码前的 DoD（Definition of Done）
进入大规模功能开发前必须满足：
1. 工程可一键构建与运行（本地 + CI）。
2. 四套构建配置可独立执行。
3. Proactor + Thread-Per-Core 主链路已达标（多 Worker、独立 `io_context`、`SO_REUSEPORT`、协程 awaitable 风格）。
4. 基础集成测试与健康检查通过。
5. 基线性能报告可生成且可比对。
6. 文档与实现引用链完整（README -> 架构分册 -> ADR）。
