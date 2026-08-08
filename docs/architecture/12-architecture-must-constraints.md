# 12. 架构 MUST 约束清单（可执行版）

本清单是对 `01-08` 与 ADR 的强制约束提炼。实现、评审、测试与发布均必须逐项对照。

## 12.1 全局运行模型
- MUST 采用 Thread-Per-Core + Shared-Nothing 作为数据面并发模型。
- MUST 保持请求主链路同核闭环，禁止在热路径引入跨核共享可变状态。
- MUST 为每个 Worker 维护独立 `io_context`，并保持 executor 亲和性可追踪。
- MUST 禁止在热路径引入阻塞系统调用。

## 12.2 协程与执行语义
- MUST 统一使用 `asio::awaitable<T>`、`co_await`、`co_spawn` 组织异步控制流。
- MUST 控制协程帧体积，禁止在跨 suspend 生命周期中持有大对象副本。
- MUST 在关键让出处（公平调度点）显式 `post`，避免热点连接长期占用执行权。

## 12.3 内存与协议栈
- MUST 采用 thread_local 无锁资源池（Buffer 与连接池）并满足借还 O(1)。
- MUST 以 RAII 管理可复用缓冲生命周期，异常路径必须自动归还。
- MUST 维持统一 RequestContext 抽象，禁止业务层直接耦合协议帧细节。
- MUST 使用 nghttp2 处理 HTTP/2，使用 llhttp 处理 HTTP/1.1。

## 12.4 路由与限流
- MUST 按 Host + Path(Trie LPM) + Client IP 三维顺序完成路由匹配。
- MUST 将路由规则编译为只读快照，运行时读取不得写锁。
- MUST 使用 thread_local 配额切分限流，禁止跨线程争用同一限流状态。
- MUST 对超限请求返回可观测拒绝结果（例如 429）并记录分路由指标。

## 12.5 Upstream 与韧性
- MUST 将 Keep-Alive 连接池按核心本地化，按 `Host:Port` 维护。
- MUST 在连接复用前进行有效性检查，失效连接必须立即销毁并重建。
- MUST 采用三态断路器（Closed/Open/Half-Open）并保证状态迁移有明确触发条件。
- MUST 将探活结果回写断路器状态，且该更新必须在一致线程上下文内完成。

## 12.6 热重载与内存模型
- MUST 使用 RCU 风格“新快照构建 -> 按核投递 -> 原子切换 -> 旧版本自然回收”流程。
- MUST 使用 store-release 发布快照、load-acquire 读取快照，并能在评审中给出 happens-before 证明。
- MUST 禁止裸指针跨协程长期持有配置快照。
- MUST 禁止在回收边界不明确时提前释放旧快照。

## 12.7 可观测与门禁
- MUST 采用 thread_local 指标采样，避免热路径全局原子竞争。
- MUST 输出至少 P50/P90/P99 延迟与错误率，并支持按路由维度观测。
- MUST 支持 W3C `traceparent` 解析与透传；缺失时必须在入口补齐 Trace 上下文。
- MUST 在 CI 或发布门禁中校验吞吐、P99、错误率三维阈值。

## 12.8 工程与治理
- MUST 在架构相关 PR 中声明影响面、回滚路径、验证证据。
- MUST 对运行模型、热重载机制、协议主栈变更同步更新 ADR。
- MUST 保持文档与实现一致；若发生偏差，必须在同一变更中修正文档或实现。

## 12.9 PR 审查最小检查表
- [ ] 数据面是否仍为同核闭环且无跨线程可变共享。
- [ ] 原子发布/读取是否标注并证明 release/acquire 同步链。
- [ ] 限流、断路、超时是否有失败路径与指标覆盖。
- [ ] 路由与配置是否仍为只读快照语义。
- [ ] 变更是否附带压测或回归证据（吞吐/P99/错误率）。
