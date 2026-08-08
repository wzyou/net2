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
