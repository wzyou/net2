# Copilot Instructions (Architecture Constitution)

本文件是本仓库 AI 生成代码时的硬约束与全局护栏。详细模块设计见 `docs/architecture/`。

## 1. 目标与边界
- 目标：构建高并发、低延迟的 C++20 协程异步网关。
- 运行模型：Thread-Per-Core + Shared-Nothing。
- 并发原则：优先同核闭环，避免跨核共享状态。

## 2. 必须遵守的架构铁律
- 禁止引入跨线程 `std::mutex` 共享临界区作为常规路径。
- 禁止在热路径引入全局可变单例与隐式共享缓存。
- 每个 Worker 持有独立 `io_context`，并保持 executor 亲和性。
- 连接从接入到转发应尽量在同一核心完成生命周期闭环。
- 热重载必须使用“发布-读取可证明安全”的指针交换模型。

## 3. 内存模型与同步
- 所有原子发布/读取必须可说明 happens-before 链。
- 配置快照发布使用 release 语义；读取使用 acquire 语义。
- 禁止使用“经验型无锁优化”替代可证明同步。

## 4. 协程与 Asio 规范
- 统一采用 `asio::awaitable<T>`、`co_await`、`co_spawn`。
- I/O 路径不得出现阻塞系统调用。
- 协程帧控制：避免大对象捕获，缩短跨 suspend 生命周期。

## 5. 路由、限流、连接池
- 路由采用 Host + Path(Trie) + Client IP 三维匹配。
- 限流默认 thread_local 配额切分，不跨线程竞争。
- Upstream 连接池按核心本地化，借还无锁。

## 6. 韧性与可观测性
- 断路器采用 Closed/Open/Half-Open 三态。
- 关键指标必须支持 P50/P90/P99 与错误率。
- Trace 传播遵循 W3C `traceparent`。

## 7. 编译与质量底线（客观真理）
- 编译器路径：见 `.vscode/settings.json`。
- 最低警告基线：`-Wall -Wextra -Wpedantic`。
- 内存安全检测：ASan/LSan；竞态检测：TSan（分构建执行）。

## 8. 文档即真理
- 任何架构变更必须同步更新 `docs/architecture/`。
- 新技术选型或方向性变更必须新增 `docs/adr/`。
- 如代码与文档冲突，以已审查通过的架构文档为准，并立即修正文档或实现。
