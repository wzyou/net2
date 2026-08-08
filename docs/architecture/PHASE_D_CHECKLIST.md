# Phase D 执行清单（程序员快速参考）

本清单是 `16-phase-d-implementation-guide.md` 的快速执行版本。完整技术细节请参考原文档。

## 前置条件检查
- [ ] Phase C 核心任务已完成（C-PROTO、C-ROUTE、C-RESILIENCE、C-UPSTREAM、C-CONFIG、C-QUALITY）
- [ ] Runtime 主链路包含：协议解析、路由、限流、断路器、连接池、热重载
- [ ] `/metrics` 端点可用且输出 Prometheus 格式
- [ ] Sanitizer 构建可执行
- [ ] 集成测试覆盖正常与错误路径

## D-BENCHMARK-BASELINE（本地基准）
**目标**：建立可重复的性能基准流程与基线数据。

### 任务清单
- [ ] 创建 `benchmarks/wrk/run_baseline.sh`（封装 wrk 执行）
- [ ] 创建 `benchmarks/wrk/baseline.lua`（请求模板）
- [ ] 创建 `benchmarks/wrk/parse_results.py`（解析 wrk 输出为 JSON）
- [ ] 创建 `benchmarks/ghz/run_baseline.sh`（gRPC 基准预留）
- [ ] 本地执行基准并记录基线数据到 `benchmarks/baseline_report.md`

### 验收标准
- [ ] 本地可一键执行 `./run_baseline.sh` 并生成 JSON 结果文件
- [ ] 结果包含：RPS、P50/P90/P99、错误率、CPU、RSS
- [ ] 至少完成"简单 GET"场景的基线数据采集

### 参考模板
参考 `16-phase-d-implementation-guide.md` 第 16.14 节附录。

---

## D-BENCHMARK-CI（性能门禁）
**目标**：在 CI 中自动执行性能基准并检测退化。

### 任务清单
- [ ] 创建 `.github/workflows/benchmark.yml`
- [ ] 创建 `scripts/ci/run_benchmark_gate.sh`（启动-执行-对比-清理）
- [ ] 创建 `scripts/ci/compare_baseline.py`（对比当前与历史基线）
- [ ] 在 CI 配置中设置门禁阈值（吞吐 -5%、P99 +10%、错误率 +0.2%）

### 验收标准
- [ ] PR 触发时 CI 自动执行基准
- [ ] 手动引入性能退化（例如热路径 sleep 1ms），CI 必须失败
- [ ] 结果 artifact 可下载并包含对比详情

---

## D-DEPLOY-SCRIPTS（部署与运维脚本）
**目标**：提供一键构建、部署、启停、健康检查脚本。

### 任务清单
- [ ] 创建 `scripts/ops/build_release.sh`（编译 Release 并打包）
- [ ] 创建 `scripts/ops/deploy.sh`（部署二进制与配置）
- [ ] 创建 `scripts/ops/start.sh`（启动网关并验证健康）
- [ ] 创建 `scripts/ops/stop.sh`（优雅停止）
- [ ] 创建 `scripts/ops/reload_config.sh`（触发热重载）
- [ ] 更新 `scripts/ops/healthcheck.sh`（确保返回明确状态码）

### 验收标准
- [ ] 本地可一键执行 `build_release.sh && deploy.sh && start.sh`
- [ ] `healthcheck.sh` 返回成功（exit 0）
- [ ] `reload_config.sh` 触发热重载且网关不重启
- [ ] `stop.sh` 优雅退出（等待现有连接处理完毕）

---

## D-CONFIG-TEMPLATES（配置模板与校验）
**目标**：补齐生产级配置模板并提供校验脚本。

### 任务清单
- [ ] 更新 `configs/gateway.dev.yaml`（覆盖开发场景）
- [ ] 更新 `configs/gateway.prod.yaml`（生产环境推荐值）
- [ ] 更新 `configs/routes.sample.yaml`（复杂路由示例）
- [ ] 创建 `configs/gateway.template.yaml`（全量配置模板与注释）
- [ ] 创建 `scripts/ops/validate_config.sh`（配置校验）

### 配置项覆盖
- [ ] 运行时：Worker 数量、端口、CPU 绑核策略
- [ ] 协议：HTTP/1.1 与 HTTP/2 开关、Header 上限
- [ ] 路由：路由规则文件路径、默认后端
- [ ] 限流：分路由配额、时间窗口、拒绝响应码
- [ ] 断路器：失败阈值、半开探测间隔、恢复阈值
- [ ] Upstream：连接池大小、超时、Keep-Alive 时长
- [ ] 可观测性：Metrics 导出端口、日志级别、Trace 采样率

### 验收标准
- [ ] 所有配置模板可被网关成功加载
- [ ] `validate_config.sh configs/gateway.prod.yaml` 返回成功
- [ ] 手动引入配置错误（例如端口号为负数），校验脚本必须失败并输出错误位置

---

## D-OPS-RUNBOOK（运维手册）
**目标**：编写可操作的运维手册。

### 任务清单
- [ ] 创建 `docs/ops/runbook.md`（运维手册主文件）
- [ ] 创建 `docs/ops/troubleshooting.md`（故障排查指南）
- [ ] 创建 `docs/ops/observability.md`（可观测性指引）

### 运维手册章节
- [ ] 部署与启动（环境要求、依赖安装、构建流程、启停流程）
- [ ] 配置管理（配置结构、热重载、校验流程）
- [ ] 可观测性（Metrics 端点、关键指标说明、日志级别、Trace 传播）
- [ ] 故障排查（至少 3 个常见场景：连接失败、超时、限流、断路器、内存增长、高 CPU）
- [ ] 升级与回滚（灰度策略、回滚流程、配置兼容性）

### 验收标准
- [ ] 运维手册可独立阅读并指导非开发人员完成部署与基础故障排查
- [ ] 文档中所有脚本路径、配置项、指标名称与实际代码一致
- [ ] 至少覆盖 3 个常见故障场景与诊断步骤

---

## D-VALIDATION（全流程验收）
**目标**：完成 Phase D 所有任务的端到端验收。

### 验收清单
- [ ] 本地可执行 `benchmarks/wrk/run_baseline.sh` 并生成基线数据
- [ ] CI 可自动执行性能基准并在退化时失败
- [ ] 可一键部署并启动网关（`build_release.sh -> deploy.sh -> start.sh`）
- [ ] 健康检查返回成功（`healthcheck.sh` 返回 0）
- [ ] 可触发配置热重载（`reload_config.sh`）
- [ ] 可优雅停止网关（`stop.sh`）
- [ ] 所有配置模板可被网关加载且 `validate_config.sh` 通过
- [ ] 运维手册覆盖部署、配置、可观测性、故障排查、升级回滚
- [ ] `docs/architecture/00-index.md` 已引用 Phase D 相关文档
- [ ] `README.md` 已更新包含基准执行与运维手册入口

### 文档对齐检查
- [ ] `11-project-scaffolding.md` Phase D 章节已更新为"已完成"状态并引用本文档
- [ ] `00-index.md` 已新增 Phase D 相关章节
- [ ] 所有新增脚本、配置、文档已在项目根 `README.md` 中可索引

---

## 执行顺序
```
D-BENCHMARK-BASELINE（本地基准）
  ↓
D-BENCHMARK-CI（性能门禁）
  ↓
D-DEPLOY-SCRIPTS（部署脚本）
  ↓
D-CONFIG-TEMPLATES（配置模板）
  ↓
D-OPS-RUNBOOK（运维手册）
  ↓
D-VALIDATION（全流程验收）
```

---

## PR 提交规范
每完成一项任务提交 PR 时必须包含：
- [ ] 对应的 D-* 任务编号
- [ ] 验收标准达成证据（本地执行截图、CI 通过链接）
- [ ] 影响的配置项、脚本、文档变更
- [ ] 对应的 MUST 条款（例如 12.7、12.8）

---

## 参考文档
- 完整实施指导：`docs/architecture/16-phase-d-implementation-guide.md`
- 架构约束清单：`docs/architecture/12-architecture-must-constraints.md`
- 项目脚手架说明：`docs/architecture/11-project-scaffolding.md`
