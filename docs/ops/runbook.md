# netp2 Gateway 运维手册

本手册提供 netp2 网关的部署、配置、监控与故障排查指引。

## 目录

- [部署与启动](#部署与启动)
- [配置管理](#配置管理)
- [可观测性](#可观测性)
- [故障排查](#故障排查)
- [升级与回滚](#升级与回滚)

---

## 部署与启动

### 环境要求

| 项目 | 最低要求 | 推荐配置 |
|------|----------|----------|
| **操作系统** | Linux/macOS | Linux 4.x+ |
| **CPU** | 2 核 | 4 核+ |
| **内存** | 2 GB | 4 GB+ |
| **磁盘** | 100 MB | 1 GB+ （含日志） |
| **编译器** | GCC 11+ / Clang 14+ | - |

### 依赖安装

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    wrk \
    python3
```

**macOS:**
```bash
brew install cmake boost wrk
```

### 构建与部署流程

```bash
# 1. 构建 Release 版本
cd /path/to/netp2
./scripts/ops/build_release.sh

# 2. 部署到目标目录
./scripts/ops/deploy.sh --dest /opt/netp2 --backup

# 3. 配置网关
cp configs/gateway.template.yaml /opt/netp2/configs/gateway.prod.yaml
vim /opt/netp2/configs/gateway.prod.yaml  # 根据环境调整

# 4. 验证配置
/opt/netp2/scripts/validate_config.sh /opt/netp2/configs/gateway.prod.yaml

# 5. 启动网关
/opt/netp2/scripts/start.sh --config /opt/netp2/configs/gateway.prod.yaml

# 6. 健康检查
/opt/netp2/scripts/healthcheck.sh http://localhost:8080/health
```

### 启动选项

```bash
/opt/netp2/scripts/start.sh [OPTIONS]

Options:
  -c, --config FILE    Config file path
  -p, --pid FILE       PID file path
  -l, --log FILE       Log file path
  -h, --help           Show help message
```

---

## 配置管理

### 配置文件结构

完整配置模板见 [configs/gateway.template.yaml](../../configs/gateway.template.yaml)

**关键配置项:**

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `server.listen` | `0.0.0.0:8080` | 监听地址 |
| `server.workers` | `0` | Worker 数量（0=自动） |
| `timeouts.connect_ms` | `5000` | 连接超时（ms） |
| `timeouts.io_idle_ms` | `30000` | I/O 空闲超时（ms） |
| `rate_limit.default_rps` | `1000` | 默认限流 RPS |
| `circuit_breaker.failure_threshold` | `5` | 断路器失败阈值 |

### 热重载配置

```bash
# 1. 修改配置文件
vim /opt/netp2/configs/gateway.prod.yaml

# 2. 验证配置
/opt/netp2/scripts/validate_config.sh /opt/netp2/configs/gateway.prod.yaml

# 3. 触发热重载
/opt/netp2/scripts/reload_config.sh

# 4. 检查日志确认重载成功
tail -f /var/log/netp2_gateway.log
```

**注意**: 当前热重载功能依赖 Phase C 配置模块完整集成。

---

## 可观测性

### Metrics 端点

**访问地址**: `http://<host>:<port>/metrics`

**输出格式**: Prometheus Text

### 关键指标说明

| 指标名称 | 类型 | 说明 |
|---------|------|------|
| `netp2_accepts_total` | Counter | 总接受的 TCP 连接数 |
| `netp2_requests_total` | Counter | 总处理的 HTTP 请求数 |
| `netp2_responses_total{code}` | Counter | HTTP 响应数（按状态码分组） |
| `netp2_bytes_in_total` | Counter | 总接收字节数 |
| `netp2_bytes_out_total` | Counter | 总发送字节数 |
| `netp2_rate_limit_rejected_total` | Counter | 限流拒绝计数（待集成） |
| `netp2_circuit_breaker_state` | Gauge | 断路器状态（待集成） |
| `netp2_upstream_connections` | Gauge | Upstream 连接池状态（待集成） |

### 查询示例

```bash
# 查看所有指标
curl http://localhost:8080/metrics

# 查看请求总数
curl http://localhost:8080/metrics | grep netp2_requests_total

# 查看错误率
curl -s http://localhost:8080/metrics | grep 'netp2_responses_total{code="[45]'
```

### 日志级别调整

修改配置文件 `observability.logging.level`:
```yaml
observability:
  logging:
    level: debug  # trace|debug|info|warn|error
```

重载配置或重启网关生效。

### Trace 传播

netp2 支持 W3C `traceparent` header 传播：

**请求头示例**:
```
traceparent: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01
```

---

## 故障排查

### 连接无法建立

**症状**: 客户端无法连接到网关

**排查步骤**:
1. 检查端口监听:
   ```bash
   netstat -tlnp | grep 8080
   # 或
   lsof -i:8080
   ```

2. 检查防火墙:
   ```bash
   sudo iptables -L -n | grep 8080
   ```

3. 检查 SO_REUSEPORT 配置:
   ```yaml
   server:
     reuse_port: true  # 必须为 true
   ```

4. 检查日志:
   ```bash
   tail -f /var/log/netp2_gateway.log
   ```

---

### 请求超时

**症状**: 请求响应缓慢或超时

**排查步骤**:
1. 检查 upstream 超时配置:
   ```yaml
   upstream:
     connect_timeout_ms: 3000  # 增加超时
   timeouts:
     request_ms: 60000  # 增加请求超时
   ```

2. 检查断路器状态:
   ```bash
   curl http://localhost:8080/metrics | grep circuit_breaker_state
   ```

3. 检查网络延迟:
   ```bash
   ping <upstream_host>
   traceroute <upstream_host>
   ```

4. 查看 Metrics P99 延迟（需基准工具）:
   ```bash
   cd benchmarks/wrk
   ./run_baseline.sh --duration 10s
   ```

---

### 限流频繁触发

**症状**: 客户端收到大量 429 响应

**排查步骤**:
1. 查看限流拒绝指标:
   ```bash
   curl http://localhost:8080/metrics | grep rate_limit_rejected
   ```

2. 调整限流配额:
   ```yaml
   rate_limit:
     default_rps: 2000  # 提升配额
   ```

3. 检查实际流量:
   ```bash
   # 查看请求速率
   curl -s http://localhost:8080/metrics | grep requests_total
   # 等待 10s 后再次查询，计算差值
   ```

---

### 断路器持续 Open

**症状**: 所有请求都被断路器拒绝

**排查步骤**:
1. 检查断路器状态:
   ```bash
   curl http://localhost:8080/metrics | grep circuit_breaker
   ```

2. 检查 upstream 健康状态:
   ```bash
   curl -I <upstream_url>
   ```

3. 调整断路器阈值:
   ```yaml
   circuit_breaker:
     failure_threshold: 10  # 增加阈值
     half_open_interval_seconds: 5  # 缩短探测间隔
   ```

4. 手动重置（重启网关）:
   ```bash
   /opt/netp2/scripts/stop.sh
   /opt/netp2/scripts/start.sh
   ```

---

### 内存持续增长

**症状**: RSS 持续上升不回落

**排查步骤**:
1. 运行 ASan 构建检测泄漏:
   ```bash
   cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DNETP2_ENABLE_ASAN=ON
   cmake --build build-asan -j
   ./build-asan/src/netp2_gateway --config configs/gateway.dev.yaml
   # 运行一段时间后停止，查看 ASan 报告
   ```

2. 检查连接池状态（Metrics）:
   ```bash
   curl http://localhost:8080/metrics | grep upstream_connections
   ```

3. 检查配置快照是否未释放（热重载频繁触发）

4. 联系开发团队分析 heap profile

---

### 高 CPU 占用

**症状**: CPU 使用率持续高于预期

**排查步骤**:
1. 检查 Worker 数量与核心数匹配:
   ```yaml
   server:
     workers: 0  # 自动匹配 CPU 核心数
   ```

2. 使用 perf 分析热点:
   ```bash
   # 需要 RelWithDebInfo 构建
   sudo perf record -g -p <gateway_pid>
   # Ctrl+C 停止采样
   sudo perf report
   ```

3. 检查是否存在跨核争用（运行 TSan）:
   ```bash
   ./scripts/ci/run-tsan.sh
   ```

4. 查看请求分布是否倾斜:
   ```bash
   curl http://localhost:8080/metrics
   # 分析各 Worker 负载是否均衡
   ```

---

## 升级与回滚

### 灰度升级策略

**方案 A：逐 Worker 替换（推荐）**

1. 构建新版本:
   ```bash
   ./scripts/ops/build_release.sh
   ```

2. 部署到临时目录:
   ```bash
   ./scripts/ops/deploy.sh --dest /opt/netp2-new
   ```

3. 启动新版本（不同端口）:
   ```bash
   # 修改配置使用不同端口（如 8081）
   /opt/netp2-new/scripts/start.sh
   ```

4. 验证新版本功能:
   ```bash
   curl http://localhost:8081/metrics
   cd benchmarks/wrk
   ./run_baseline.sh --port 8081
   ```

5. 切换流量（Load Balancer 层）

6. 停止旧版本:
   ```bash
   /opt/netp2/scripts/stop.sh
   ```

**方案 B：直接替换**

1. 停止旧版本:
   ```bash
   /opt/netp2/scripts/stop.sh
   ```

2. 备份并部署新版本:
   ```bash
   ./scripts/ops/deploy.sh --backup
   ```

3. 启动新版本:
   ```bash
   /opt/netp2/scripts/start.sh
   ```

### 回滚流程

**前提**: 部署时使用了 `--backup` 选项

1. 停止当前版本:
   ```bash
   /opt/netp2/scripts/stop.sh
   ```

2. 恢复备份:
   ```bash
   BACKUP_DIR=$(ls -td /opt/netp2.backup.* | head -1)
   sudo rm -rf /opt/netp2
   sudo mv $BACKUP_DIR /opt/netp2
   ```

3. 启动旧版本:
   ```bash
   /opt/netp2/scripts/start.sh
   ```

4. 验证健康:
   ```bash
   /opt/netp2/scripts/healthcheck.sh
   ```

---

## 附录

### 快速命令参考

```bash
# 启动
/opt/netp2/scripts/start.sh

# 停止
/opt/netp2/scripts/stop.sh

# 重载配置
/opt/netp2/scripts/reload_config.sh

# 健康检查
/opt/netp2/scripts/healthcheck.sh

# 查看 Metrics
curl http://localhost:8080/metrics

# 查看日志
tail -f /var/log/netp2_gateway.log

# 验证配置
/opt/netp2/scripts/validate_config.sh <config_file>
```

### 相关文档

- 架构文档: [docs/architecture/](../architecture/)
- 配置模板: [configs/gateway.template.yaml](../../configs/gateway.template.yaml)
- 性能基准: [benchmarks/baseline_report.md](../../benchmarks/baseline_report.md)
- 实施指导: [docs/architecture/16-phase-d-implementation-guide.md](../architecture/16-phase-d-implementation-guide.md)

---

*最后更新：2026-08-08*  
*维护团队：netp2 开发组*
