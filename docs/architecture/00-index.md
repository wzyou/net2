# Architecture Index

本文档集采用“总分结合”：
- 总纲：`.github/copilot-instructions.md`（全局铁律，精简）
- 分册：`docs/architecture/*.md`（模块契约，可深挖）

## 阅读顺序
1. `01-overview.md`
2. `02-core-runtime.md`
3. `03-memory-and-protocol.md`
4. `04-routing-and-rate-limit.md`
5. `05-upstream-and-resilience.md`
6. `06-hot-reload-rcu.md`
7. `07-observability-and-fairness.md`
8. `08-deployment-and-tuning.md`
9. `09-evolution-matrix.md`
10. `10-request-flow-pseudocode.md`
11. `11-project-scaffolding.md`
12. `12-architecture-must-constraints.md`
13. `13-must-implementation-mapping.md`
14. `14-phase-c-implementation-guide.md`
15. `15-phase-c-quality-validation.md` ← **Phase C 质量验证（ASan/TSan）**
16. `16-phase-d-implementation-guide.md` ← **Phase D 性能基准与运维就绪**

## 范围声明
- 本目录由“高性能 C++20 协程异步网关 v1.0 全量技术设计”拆分整理而来。
- 若后续实现与文档不一致，必须在 PR 中明确偏差、原因与修复计划。
