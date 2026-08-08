# wrk benchmarks

HTTP 基准脚本与结果归档。

## 快速开始

```bash
# 1. 启动网关（RelWithDebInfo 构建推荐）
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./build/src/netp2_gateway --config configs/gateway.dev.yaml &

# 2. 等待网关就绪
sleep 3

# 3. 执行基准测试
cd benchmarks/wrk
./run_baseline.sh --duration 30s --threads 4 --connections 100

# 4. 查看结果
cat baseline_*.json
```

## 使用说明

```bash
./run_baseline.sh [options]

Options:
  --host HOST         Target host (default: localhost)
  --port PORT         Target port (default: 18080)
  --duration DURATION Test duration (default: 30s)
  --threads THREADS   Thread count (default: 4)
  --connections CONN  Connection count (default: 100)
  --output FILE       Output JSON file
  -h, --help          Show help message
```

## 输出指标

- **rps**: 平均每秒请求数
- **p50_ms**: P50 延迟（毫秒）
- **p75_ms**: P75 延迟（毫秒）
- **p90_ms**: P90 延迟（毫秒）
- **p99_ms**: P99 延迟（毫秒）
- **error_rate**: 错误率（百分比）
- **total_requests**: 总请求数
- **error_count**: 错误请求数
- **transfer_mbps**: 吞吐量（MB/s）

## 基线数据

完整基线报告见：[../baseline_report.md](../baseline_report.md)

## 参考

- 实施指导：`docs/architecture/16-phase-d-implementation-guide.md` 第 16.3 节
- 执行清单：`docs/architecture/PHASE_D_CHECKLIST.md`

