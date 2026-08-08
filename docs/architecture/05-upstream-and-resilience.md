# 05. Upstream 连接池与高可用防护

## 5.1 Thread-Local Keep-Alive Pool
- 每个 Core 按 `Host:Port` 维护本地空闲连接队列。
- 借还无锁，避免跨线程同步。
- 默认策略：
1. Max Idle Timeout: 60s
2. Max Requests Per Connection: 10000

## 5.2 连接健康与剔除
- 复用前可进行非阻塞探测（如 peek）判断远端关闭。
- 失效连接立即销毁并重建，避免脏连接传播。

## 5.3 三态断路器（Closed/Open/Half-Open）
- Closed：正常放行。
- Open：快速失败并拒绝新请求。
- Half-Open：小流量探测恢复能力。

```mermaid
stateDiagram-v2
    [*] --> Closed
    Closed --> Open: error_rate > threshold
    Open --> HalfOpen: cooldown elapsed
    HalfOpen --> Closed: success N times
    HalfOpen --> Open: any probe failed
```

## 5.4 主动探活协程
- Closed 态低频探活（如 15s）。
- Open 态高频探活（如 2s）以缩短恢复时间。
- 探活结果驱动断路器状态迁移。
