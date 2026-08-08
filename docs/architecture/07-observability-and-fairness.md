# 07. 可观测性与调度公平性

## 7.1 Thread-Local Metrics
- Counter/Gauge/Histogram 以 thread_local 采样写入。
- 避免热路径原子竞争和伪共享。

## 7.2 HDR Histogram
- 覆盖微秒到秒级大动态范围。
- 重点输出 P50/P90/P99/P999 尾延迟。

## 7.3 Prometheus 读时聚合
- 抓取 `/metrics` 时聚合各 Core 指标。
- 聚合策略要求无写锁干扰数据面。

## 7.4 OpenTelemetry Tracing
- 解析与透传 `traceparent`。
- 缺失时入口生成 TraceID。
- Span 异步批量导出到 OTel Collector。

## 7.5 公平调度与超时
- 对热点连接设置 `MAX_CONSECUTIVE_READS` 上限。
- 触发上限后 `co_await asio::post(...)` 主动让出执行权。
- 建连超时与读写超时采用 awaitable 组合竞态实现。

## 7.6 MUST 约束核对项
- MUST 采用 thread_local 指标采样，禁止热路径全局写锁或高频原子争用。
- MUST 输出至少 P50/P90/P99 延迟、错误率与分路由统计。
- MUST 保证 `/metrics` 聚合读取不阻塞数据面写入路径。
- MUST 支持 W3C `traceparent` 解析与透传；缺失时入口补齐 Trace 上下文。
- MUST 在公平调度点显式让出执行权并保留超时保护。
