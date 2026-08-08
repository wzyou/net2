# 16. Phase D 实施指导（Architect -> Developer）

本文面向 Phase D（Week 3+）开发执行。目标是建立**可重复、可对比的性能门禁体系**，并完成生产就绪所需的运维支撑。

## 16.1 前置条件（DoR: Definition of Ready）
进入 Phase D 前必须满足：
- Phase C 核心任务已完成：C-PROTO、C-ROUTE、C-RESILIENCE、C-UPSTREAM、C-CONFIG、C-QUALITY。
- Runtime 主链路已包含：HTTP/1.1 解析、路由匹配、限流、断路器、upstream 连接池、热重载。
- 已有 thread_local metrics 与 `/metrics` Prometheus 导出端点。
- Sanitizer（ASan/LSan/TSan）构建与 race 测试可执行。
- 集成测试覆盖正常路径与核心错误路径。

若 Phase C 未达标，禁止进入 Phase D。

## 16.2 总体执行顺序
Phase D 必须按以下顺序推进：

1. **D-BENCHMARK-BASELINE**：建立本地基准脚本与基线数据采集流程。
2. **D-BENCHMARK-CI**：将基准集成到 CI，建立性能回归门禁。
3. **D-DEPLOY-SCRIPTS**：补齐构建、部署、启停、健康检查脚本。
4. **D-CONFIG-TEMPLATES**：补齐生产级配置模板与配置校验。
5. **D-OPS-RUNBOOK**：编写运维手册，包含故障排查与可观测性指引。
6. **D-VALIDATION**：全流程验收与文档对齐。

---

## 16.3 D-BENCHMARK-BASELINE：建立性能基线
命中 MUST：`12.7 可观测与门禁`。

### 16.3.1 目标
- 建立可重复的 HTTP/gRPC 性能基准流程。
- 采集吞吐（RPS）、延迟分布（P50/P90/P99）、错误率、CPU/内存占用。
- 形成基线数据作为后续回归检测的对比基准。

### 16.3.2 代码落点
- `benchmarks/wrk/run_baseline.sh`：封装 wrk 执行与结果解析。
- `benchmarks/wrk/baseline.lua`：定义 wrk 请求模板（支持不同路由场景）。
- `benchmarks/wrk/parse_results.py`：解析 wrk 输出为结构化 JSON。
- `benchmarks/ghz/run_baseline.sh`：封装 ghz 执行与结果解析（预留 gRPC 支持）。
- `benchmarks/baseline_report.md`：记录基线数据与环境信息。

### 16.3.3 基准场景定义
至少覆盖以下场景：
1. **简单 GET**：路由命中，无限流，响应 200 OK。
2. **限流触发**：高并发触发限流，部分请求返回 429。
3. **断路器触发**：模拟 upstream 失败，触发断路器 Open 状态。
4. **热重载期间**：触发配置热重载，观测请求延迟抖动。

### 16.3.4 数据采集要求
每个场景必须记录：
- 吞吐：平均 RPS。
- 延迟：P50、P90、P99、P99.9（单位：ms）。
- 错误率：(失败请求数 / 总请求数) * 100%。
- 资源占用：CPU %（多核平均）、RSS（MB）。
- 环境信息：CPU 型号、核心数、系统、编译器版本、构建类型。

### 16.3.5 本地执行流程
```bash
# Step 1: 构建 Release 版本
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

# Step 2: 启动网关
./build/src/app/netp2_gateway --config configs/gateway.dev.yaml &
GATEWAY_PID=$!

# Step 3: 等待就绪
sleep 2
curl -f http://localhost:8080/health || exit 1

# Step 4: 执行基准
cd benchmarks/wrk
./run_baseline.sh --host localhost --port 8080 --output baseline_$(date +%Y%m%d).json

# Step 5: 停止网关
kill -TERM $GATEWAY_PID
wait $GATEWAY_PID
```

### 16.3.6 验收标准
- 本地可一键执行 `run_baseline.sh` 并生成结构化结果文件。
- 结果文件包含 RPS、P50/P90/P99、错误率、CPU、RSS。
- 至少完成"简单 GET"场景的基线数据采集并记录到 `baseline_report.md`。

---

## 16.4 D-BENCHMARK-CI：性能回归门禁
命中 MUST：`12.7 可观测与门禁`。

### 16.4.1 目标
- 在 CI 中自动执行性能基准并与基线对比。
- 若关键指标退化超过阈值，失败并阻止合并。

### 16.4.2 代码落点
- `.github/workflows/benchmark.yml`：定义 CI 基准流水线。
- `scripts/ci/run_benchmark_gate.sh`：封装启动、执行、对比、清理逻辑。
- `scripts/ci/compare_baseline.py`：对比当前运行结果与历史基线，判断是否退化。

### 16.4.3 门禁阈值（建议）
参考 `11-project-scaffolding.md` 第 11.5 节：
- 吞吐下降 > 5% → 失败。
- P99 延迟上升 > 10% → 失败。
- 错误率上升 > 基线 + 0.2% → 失败。

阈值可根据实际基线数据调整，但必须在 CI 配置或脚本中显式声明。

### 16.4.4 CI 流水线结构
```yaml
name: Performance Benchmark

on:
  pull_request:
    branches: [main, develop]
  schedule:
    - cron: '0 2 * * 0'  # 每周日凌晨 2 点

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y wrk
      - name: Build Release
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
          cmake --build build -j$(nproc)
      - name: Run Benchmark Gate
        run: ./scripts/ci/run_benchmark_gate.sh
      - name: Upload Results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: benchmarks/wrk/*.json
```

### 16.4.5 验收标准
- CI 可自动触发基准流程并在 PR 中显示结果。
- 若手动引入性能退化（例如在热路径添加 `std::this_thread::sleep_for(1ms)`），CI 必须失败。
- 结果 artifact 可下载并包含对比详情。

---

## 16.5 D-DEPLOY-SCRIPTS：部署与运维脚本
命中 MUST：`12.8 工程与治理`。

### 16.5.1 目标
- 提供一键构建、部署、启停、健康检查脚本。
- 支持生产环境常见操作（优雅启停、热重载、日志轮转）。

### 16.5.2 代码落点
- `scripts/ops/build_release.sh`：编译 Release 版本并打包。
- `scripts/ops/deploy.sh`：部署二进制与配置到目标目录。
- `scripts/ops/start.sh`：启动网关并验证健康检查。
- `scripts/ops/stop.sh`：优雅停止网关。
- `scripts/ops/reload_config.sh`：触发配置热重载（发送 SIGHUP 或调用 API）。
- `scripts/ops/healthcheck.sh`：已存在，需确保返回明确状态码与响应。

### 16.5.3 脚本规范
- 所有脚本必须支持 `-h` 参数输出用法说明。
- 失败时必须返回非零退出码。
- 日志输出必须包含时间戳与明确操作描述。
- 操作前必须检查前置条件（例如进程是否已运行、配置文件是否存在）。

### 16.5.4 健康检查要求
`healthcheck.sh` 必须返回：
- `0`：健康。
- `1`：不健康（进程未启动或 `/health` 返回非 200）。
- `2`：未知（例如网络不可达）。

响应必须包含：
- 状态：`ok` 或 `error`。
- 版本：可选，便于追溯部署版本。
- Worker 数量：便于验证多核配置。

### 16.5.5 验收标准
- 本地可一键执行 `scripts/ops/build_release.sh && scripts/ops/deploy.sh && scripts/ops/start.sh`。
- 启动后 `healthcheck.sh` 返回成功。
- 执行 `reload_config.sh` 后，网关加载新配置且无重启（通过日志验证）。
- 执行 `stop.sh` 后，网关优雅退出（等待现有连接处理完毕）。

---

## 16.6 D-CONFIG-TEMPLATES：生产级配置模板
命中 MUST：`12.8 工程与治理`。

### 16.6.1 目标
- 补齐生产级配置模板并附带注释说明。
- 提供配置校验脚本，避免部署时因配置错误导致启动失败。

### 16.6.2 代码落点
- `configs/gateway.dev.yaml`：已存在，确保覆盖开发所需场景。
- `configs/gateway.prod.yaml`：已存在，需补充生产环境推荐值。
- `configs/routes.sample.yaml`：已存在，需补充复杂路由场景示例。
- `configs/gateway.template.yaml`：全量配置模板，包含所有可配置项与注释。
- `scripts/ops/validate_config.sh`：配置校验脚本。

### 16.6.3 配置模板要求
至少覆盖以下配置项：
- **运行时**：Worker 数量、端口、CPU 绑核策略。
- **协议**：HTTP/1.1 与 HTTP/2 开关、Header 大小上限、请求数量上限。
- **路由**：路由规则文件路径、默认后端。
- **限流**：分路由配额、时间窗口、拒绝响应码。
- **断路器**：失败阈值、半开探测间隔、恢复阈值。
- **Upstream**：连接池大小、超时、Keep-Alive 时长。
- **可观测性**：Metrics 导出端口、日志级别、Trace 采样率。

### 16.6.4 配置校验要求
`validate_config.sh` 必须检查：
- YAML 格式有效性。
- 必填字段是否存在（例如 `listen_port`、`routes_file`）。
- 数值范围合理性（例如 `worker_count > 0`）。
- 路由文件引用路径是否存在。

失败时必须输出明确错误信息与行号。

### 16.6.5 验收标准
- 所有配置模板可被网关成功加载。
- `validate_config.sh configs/gateway.prod.yaml` 返回成功。
- 手动引入配置错误（例如端口号为负数），`validate_config.sh` 必须失败并输出错误位置。

---

## 16.7 D-OPS-RUNBOOK：运维手册
命中 MUST：`12.8 工程与治理`。

### 16.7.1 目标
- 提供可操作的运维手册，覆盖常见故障场景与诊断步骤。
- 明确可观测性入口与关键指标解读。

### 16.7.2 代码落点
- `docs/ops/runbook.md`：运维手册主文件。
- `docs/ops/troubleshooting.md`：故障排查指南。
- `docs/ops/observability.md`：可观测性指引。

### 16.7.3 运维手册章节
至少包含以下内容：

#### 16.7.3.1 部署与启动
- 环境要求（OS、CPU、内存）。
- 依赖安装（Boost、nghttp2、llhttp）。
- 构建与部署流程（引用 `scripts/ops/`）。
- 启动与停止流程（优雅停止、信号处理）。

#### 16.7.3.2 配置管理
- 配置文件结构说明（引用 `configs/gateway.template.yaml`）。
- 热重载操作（`reload_config.sh` 或 SIGHUP）。
- 配置校验流程（`validate_config.sh`）。

#### 16.7.3.3 可观测性
- Metrics 端点：`http://<host>:<metrics_port>/metrics`。
- 关键指标说明：
  - `netp2_requests_total`：按路由分组的请求总数。
  - `netp2_requests_duration_seconds`：按路由分组的延迟分布（P50/P90/P99）。
  - `netp2_rate_limit_rejected_total`：限流拒绝计数。
  - `netp2_circuit_breaker_state`：断路器状态（Closed=0, Open=1, HalfOpen=2）。
  - `netp2_upstream_connections`：upstream 连接池状态。
- 日志级别调整。
- Trace 上下文传播与调试。

#### 16.7.3.4 故障排查
- **连接无法建立**：检查端口监听、防火墙、`SO_REUSEPORT` 配置。
- **请求超时**：检查 upstream 超时配置、断路器状态、网络延迟。
- **限流频繁触发**：检查配额配置、实际流量、时间窗口。
- **断路器持续 Open**：检查 upstream 健康状态、探活间隔、恢复阈值。
- **内存持续增长**：检查连接池泄漏、配置快照未释放、协程帧泄漏（运行 ASan 版本）。
- **高 CPU 占用**：检查热点路径（使用 perf 或 profiler）、协程调度公平性、跨核争用。

#### 16.7.3.5 升级与回滚
- 灰度升级策略（例如按 Worker 逐步替换）。
- 回滚流程（保留旧版本二进制，切换符号链接）。
- 配置兼容性检查（新旧配置格式差异）。

### 16.7.4 验收标准
- 运维手册可独立阅读并指导非开发人员完成部署与基础故障排查。
- 文档中所有脚本路径、配置项、指标名称与实际代码一致。
- 至少覆盖 3 个常见故障场景与诊断步骤。

---

## 16.8 D-VALIDATION：全流程验收
命中 MUST：`12.8 工程与治理`。

### 16.8.1 目标
- 完成 Phase D 所有任务的端到端验收。
- 确保文档与实现对齐。

### 16.8.2 验收清单
- [ ] 本地可执行 `benchmarks/wrk/run_baseline.sh` 并生成基线数据。
- [ ] CI 可自动执行性能基准并在退化时失败。
- [ ] 可一键部署并启动网关（`build_release.sh -> deploy.sh -> start.sh`）。
- [ ] 健康检查返回成功（`healthcheck.sh` 返回 0）。
- [ ] 可触发配置热重载（`reload_config.sh`）。
- [ ] 可优雅停止网关（`stop.sh`）。
- [ ] 所有配置模板可被网关加载且 `validate_config.sh` 通过。
- [ ] 运维手册覆盖部署、配置、可观测性、故障排查、升级回滚。
- [ ] `docs/architecture/00-index.md` 已引用 Phase D 相关文档。
- [ ] `README.md` 已更新包含基准执行与运维手册入口。

### 16.8.3 文档对齐检查
- `11-project-scaffolding.md` Phase D 章节已更新为"已完成"状态并引用本文档。
- `00-index.md` 已新增 Phase D 相关章节。
- 所有新增脚本、配置、文档已在项目根 `README.md` 中可索引。

---

## 16.9 Phase D 执行顺序总结
```text
D-BENCHMARK-BASELINE（本地基准脚本与基线数据）
  ↓
D-BENCHMARK-CI（CI 性能回归门禁）
  ↓
D-DEPLOY-SCRIPTS（部署与运维脚本）
  ↓
D-CONFIG-TEMPLATES（生产级配置模板与校验）
  ↓
D-OPS-RUNBOOK（运维手册与故障排查）
  ↓
D-VALIDATION（全流程验收与文档对齐）
```

---

## 16.10 Phase D 完成后的系统状态
- 性能基线已建立并可自动回归检测。
- 部署与运维流程可重复且有文档支撑。
- 运维人员可独立完成部署、配置、故障排查。
- CI 门禁覆盖功能测试、内存安全、竞态检测、性能回归。
- 系统具备生产就绪条件（Performance Ready + Ops Ready）。

---

## 16.11 与架构约束的对应关系
- `12.7 可观测与门禁`：通过 D-BENCHMARK-BASELINE 与 D-BENCHMARK-CI 落地。
- `12.8 工程与治理`：通过 D-DEPLOY-SCRIPTS、D-CONFIG-TEMPLATES、D-OPS-RUNBOOK 落地。
- `12.1-12.6 运行模型、协程、内存、路由、Upstream、热重载`：在 Phase D 不做新增变更，仅通过性能基准与运维手册验证已有实现符合约束。

---

## 16.12 常见问题与风险
### Q1：基准结果波动较大怎么办？
- 确保基准环境稳定（关闭 CPU 动态调频、避免后台任务干扰）。
- 多次运行取中位数或平均值。
- 使用 RelWithDebInfo 而非 Debug 构建。
- 在 CI 中使用固定规格的 runner。

### Q2：CI 基准运行时间过长怎么办？
- 缩短基准时长（例如从 60s 降至 30s）但保持场景覆盖。
- 仅在 PR 合并后或定期（每周）执行全量基准，PR 阶段执行快速烟测。

### Q3：性能退化阈值如何确定？
- 初期可设置较宽松阈值（例如吞吐 -10%、P99 +20%）。
- 随着基线稳定性提升逐步收紧阈值。
- 区分预期变更（例如引入新功能）与意外退化。

### Q4：运维手册如何保持同步？
- 每次变更配置项、指标名称、脚本路径时同步更新运维手册。
- 在 PR 审查中检查文档一致性（纳入 `12.9 PR 审查最小检查表`）。

---

## 16.13 进入下一阶段的条件
若 Phase D 全部完成且验收通过，系统已具备：
- 可重复的性能基线与回归检测。
- 可操作的部署与运维流程。
- 可独立排查故障的运维手册。

此时可进入：
- **Phase E（可选）**：高级特性（HTTP/2 推送、动态 Upstream 发现、分布式追踪）。
- **生产试运行**：小流量灰度验证。
- **性能优化专项**：基于基准数据定位瓶颈并优化。

---

## 16.14 附录：基准脚本模板
### 16.14.1 `benchmarks/wrk/run_baseline.sh`
```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST="localhost"
PORT="8080"
DURATION="30s"
THREADS="4"
CONNECTIONS="100"
OUTPUT=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --duration) DURATION="$2"; shift 2 ;;
    --threads) THREADS="$2"; shift 2 ;;
    --connections) CONNECTIONS="$2"; shift 2 ;;
    --output) OUTPUT="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --host HOST         Target host (default: localhost)"
      echo "  --port PORT         Target port (default: 8080)"
      echo "  --duration DURATION Test duration (default: 30s)"
      echo "  --threads THREADS   Thread count (default: 4)"
      echo "  --connections CONN  Connection count (default: 100)"
      echo "  --output FILE       Output JSON file"
      exit 0
      ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

if [[ -z "$OUTPUT" ]]; then
  OUTPUT="$SCRIPT_DIR/baseline_$(date +%Y%m%d_%H%M%S).json"
fi

echo "[$(date +%T)] Running wrk baseline..."
echo "  Target: http://$HOST:$PORT"
echo "  Duration: $DURATION, Threads: $THREADS, Connections: $CONNECTIONS"

wrk -t "$THREADS" -c "$CONNECTIONS" -d "$DURATION" \
  -s "$SCRIPT_DIR/baseline.lua" \
  "http://$HOST:$PORT/proxy/mock" \
  > "$SCRIPT_DIR/wrk_output.txt"

echo "[$(date +%T)] Parsing results..."
python3 "$SCRIPT_DIR/parse_results.py" \
  "$SCRIPT_DIR/wrk_output.txt" \
  "$OUTPUT"

echo "[$(date +%T)] Results saved to: $OUTPUT"
cat "$OUTPUT"
```

### 16.14.2 `benchmarks/wrk/baseline.lua`
```lua
request = function()
  headers = {
    ["Host"] = "example.com",
    ["User-Agent"] = "wrk-benchmark"
  }
  return wrk.format("GET", "/proxy/mock", headers, nil)
end
```

### 16.14.3 `benchmarks/wrk/parse_results.py`
```python
#!/usr/bin/env python3
import sys
import json
import re

def parse_wrk_output(text):
    result = {}
    
    # Parse Requests/sec
    match = re.search(r'Requests/sec:\s+([\d.]+)', text)
    if match:
        result['rps'] = float(match.group(1))
    
    # Parse Latency distribution
    p50_match = re.search(r'50%\s+([\d.]+)(ms|s)', text)
    p90_match = re.search(r'90%\s+([\d.]+)(ms|s)', text)
    p99_match = re.search(r'99%\s+([\d.]+)(ms|s)', text)
    
    def convert_to_ms(value, unit):
        return float(value) if unit == 'ms' else float(value) * 1000
    
    if p50_match:
        result['p50_ms'] = convert_to_ms(p50_match.group(1), p50_match.group(2))
    if p90_match:
        result['p90_ms'] = convert_to_ms(p90_match.group(1), p90_match.group(2))
    if p99_match:
        result['p99_ms'] = convert_to_ms(p99_match.group(1), p99_match.group(2))
    
    # Parse error rate (Non-2xx or 3xx responses)
    error_match = re.search(r'Non-2xx or 3xx responses:\s+(\d+)', text)
    total_match = re.search(r'(\d+) requests in', text)
    if error_match and total_match:
        errors = int(error_match.group(1))
        total = int(total_match.group(1))
        result['error_rate'] = (errors / total * 100) if total > 0 else 0
    else:
        result['error_rate'] = 0
    
    return result

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <wrk_output.txt> <output.json>")
        sys.exit(1)
    
    with open(sys.argv[1], 'r') as f:
        wrk_output = f.read()
    
    result = parse_wrk_output(wrk_output)
    
    with open(sys.argv[2], 'w') as f:
        json.dump(result, f, indent=2)
    
    print(json.dumps(result, indent=2))
```

---

## 16.15 下一步
程序员应按照 16.2 的执行顺序逐项完成任务，每完成一项提交 PR 并在描述中关联：
- 对应的 D-* 任务编号。
- 验收标准达成证据（例如本地执行截图、CI 通过链接）。
- 影响的配置项、脚本、文档变更。

所有 Phase D 任务完成后，更新 `11-project-scaffolding.md` 将 Phase D 标记为"已完成"，并新增 Phase D 执行总结。
