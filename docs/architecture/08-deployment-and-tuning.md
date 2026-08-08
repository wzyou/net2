# 08. 部署与内核调优

## 8.1 系统参数（建议基线）
```conf
fs.file-max = 2097152
net.core.somaxconn = 65536
net.ipv4.tcp_max_syn_backlog = 65536
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_slow_start_after_idle = 0
```

## 8.2 编译优化建议
- 编译器：GCC 11+ 或 Clang 13+。
- 优化选项：`-O3 -flto -march=native`（按环境评估）。
- 建议评估 jemalloc 对杂项分配路径的收益。

## 8.3 运行时优化建议
- Worker 线程设置 CPU Affinity，降低跨核迁移。
- 结合负载模型进行连接池、限流与超时参数校准。

## 8.4 验证策略
- 压测：HTTP 用 wrk，gRPC 用 ghz。
- 安全：ASan/LSan、TSan 分构建执行。
- 门禁：吞吐、P99、错误率三维阈值。
