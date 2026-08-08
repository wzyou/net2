# 02. 核心运行模型与网络架构

## 2.1 Thread-Per-Core + SO_REUSEPORT
- 每个物理核心对应一个 Worker 线程。
- 每个 Worker 持有独立 `boost::asio::io_context`，`concurrency_hint=1`。
- 各 Worker 在同端口上独立 `accept`，由内核按 4-tuple Hash 分发新连接。

## 2.2 生命周期同核闭环
- 请求从 accept 到协议解析、路由、限流、转发、指标上报尽量在同核完成。
- 避免跨核队列和共享缓存抖动，提高 L1/L2 命中率。

## 2.3 Proactor 与协程调度
- I/O 发起时协程挂起，完成后由事件循环恢复。
- 开发模型保持线性同步风格，执行模型保持异步事件驱动。

## 2.4 协程帧与执行规范
- 协程帧应控制在轻量级，避免捕获大对象。
- suspend 点之间只保留必要状态，降低内存与恢复开销。
- executor 绑定必须明确，避免隐式跨 executor 切换。

## 2.5 公平性基础
- 对高频热点连接设置连续读取上限。
- 达到阈值后通过 `asio::post` 主动让出执行权，降低饥饿风险。

## 2.6 MUST 约束核对项
- MUST 每核仅运行一个 Worker 事件循环，并保持 `io_context` 独立。
- MUST 请求主链路在同核完成，不得引入跨核共享可变状态作为常规路径。
- MUST 显式绑定 executor，禁止隐式跨 executor 切换。
- MUST 在热点连接上启用公平让出机制，防止协程饥饿。
- MUST 在 I/O 主路径禁用阻塞系统调用。
