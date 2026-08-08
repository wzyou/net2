# 13. MUST 约束到实现责任模块映射表

本文将 `12-architecture-must-constraints.md` 的 MUST 条款映射到当前工程目录，作为开发排期、代码评审与测试验收的统一对照表。

## 13.1 使用方式
- 实现阶段：每个任务必须先标注命中的 MUST 条款与主责模块。
- 评审阶段：PR 必须附带“条款 -> 代码位置 -> 验证证据”的闭环说明。
- 验收阶段：按本表逐行核对，未闭环条款不得标记完成。

## 13.2 映射矩阵（MUST）

| MUST 分组 | 关键约束摘要 | 主责实现模块 | 协作模块 | 最小验收证据 | 当前状态 |
|---|---|---|---|---|---|
| 12.1 全局运行模型 | TPC + Shared-Nothing；同核闭环；独立 io_context；热路径无阻塞 | src/runtime/, include/netp2/runtime/ | src/app/, src/protocol/ | tests/integration/ + 压测报告（同核闭环与错误率） | 骨架已建，细节待实现 |
| 12.2 协程与执行语义 | 统一 awaitable/co_await/co_spawn；控制协程帧；显式公平让出 | src/runtime/, src/app/ | src/protocol/, src/upstream/ | 单元/集成用例覆盖超时与让出路径 | 骨架已建，细节待实现 |
| 12.3 内存与协议栈 | thread_local 无锁池 + RAII；RequestContext 统一；nghttp2 + llhttp | src/protocol/, src/upstream/, include/netp2/core/ | src/runtime/, src/config/ | 协议解析与资源回收测试；构建配置审计输出 | 协议方向已定，主体待实现 |
| 12.4 路由与限流 | Host+Path+IP 三维匹配；只读快照；thread_local 配额；429 可观测 | src/routing/, src/resilience/ | src/config/, src/observability/ | tests/unit/ + tests/integration/ 的命中/拒绝路径 | 骨架已建，细节待实现 |
| 12.5 Upstream 与韧性 | 核本地连接池；复用前健康检查；三态断路；探活回写 | src/upstream/, src/resilience/ | src/runtime/, src/observability/ | 集成测试覆盖 503/504 与恢复路径 | 骨架已建，细节待实现 |
| 12.6 热重载与内存模型 | RCU 快照切换；release/acquire；禁止裸指针跨协程 | src/config/, src/routing/ | src/runtime/, src/resilience/ | tests/race/ + 文档化 happens-before 证明 | 骨架已建，细节待实现 |
| 12.7 可观测与门禁 | thread_local 指标；P50/P90/P99/错误率；traceparent；CI 门禁 | src/observability/, scripts/ci/ | tests/integration/, benchmarks/ | Prometheus 指标检查 + CI 阈值判定记录 | 骨架已建，细节待实现 |
| 12.8 工程与治理 | PR 影响面/回滚/证据；ADR 同步；文档实现一致 | docs/architecture/, docs/adr/ | README.md, scripts/ci/ | PR 模板与审查记录；ADR 变更链路 | 文档已落地，流程待固化 |

## 13.3 目录责任细化
- runtime: Worker 生命周期、acceptor、executor 亲和、公平调度点。
- protocol: HTTP1.1/HTTP2 解析接入与 RequestContext 归一。
- routing: Host/Path/IP 匹配、路由快照读取。
- resilience: 限流、断路器、探活状态迁移。
- upstream: 本地连接池、建连超时、复用健康检查。
- config: 配置加载、编译、按核发布与 RCU 指针切换。
- observability: thread_local metrics、直方图、trace 透传、聚合导出。

## 13.4 验收清单（执行时必须附证据）
- MUST 为每条变更标注命中的 12.x 条款编号。
- MUST 提供至少一条可复现的验证命令或测试入口。
- MUST 在性能敏感改动中附吞吐、P99、错误率对比。
- MUST 在并发/热更改动中附同步语义证明与回收边界说明。

## 13.5 当前可追踪锚点（仓库内已存在）
- 文档总清单：docs/architecture/12-architecture-must-constraints.md
- 架构索引：docs/architecture/00-index.md
- 运行与模块说明：src/runtime/README.md、src/protocol/README.md、src/routing/README.md、src/resilience/README.md、src/upstream/README.md、src/config/README.md、src/observability/README.md
- 测试入口说明：tests/unit/README.md、tests/integration/README.md、tests/race/README.md、tests/e2e/README.md
- 基础脚本：scripts/ci/build_and_test.sh、scripts/dev/build.sh、scripts/ops/healthcheck.sh
