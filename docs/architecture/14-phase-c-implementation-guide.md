# 14. Phase C 实施指导（Architect -> Developer）

本文面向 Phase C（Week 2）开发执行。目标不是一次性完成所有网关能力，而是把 Phase B 的最小闭环升级为符合 MUST 约束的可演进主链路。

## 14.1 总体执行顺序
Phase C 必须按以下顺序推进：

1. C-PROTO：先完成协议解析与 `RequestContext` 归一。
2. C-ROUTE：再替换临时路由为 Host + Path + Client IP 匹配。
3. C-RESILIENCE：接入 thread_local 限流与三态断路器。
4. C-UPSTREAM：接入按核心本地化的 upstream Keep-Alive 连接池。
5. C-CONFIG：接入 RCU 热重载与只读快照发布。
6. C-QUALITY：补齐 sanitizer、race、集成测试与验收证据。

禁止在 C-PROTO 完成前继续扩大 `RuntimeSpine` 内部的临时解析逻辑。`RuntimeSpine` 只负责连接生命周期、executor 亲和、协程调度与流水线编排；协议细节必须迁移到 `src/protocol/`。

## 14.2 C-PROTO：协议解析第一优先级
命中 MUST：`12.3 内存与协议栈`、`12.2 协程与执行语义`。

### 代码落点
- 新增 `include/netp2/protocol/`：对外暴露 `RequestContext`、解析结果、错误码与 HTTP codec 接口。
- 新增 `src/protocol/` 实现文件：HTTP/1.1 使用 `llhttp`，HTTP/2 预留 `nghttp2` 适配入口。
- 更新 `src/CMakeLists.txt`：将 protocol 实现纳入 `netp2_core`。
- 新增 `tests/unit/http1_codec_test.cpp`：覆盖解析成功与协议错误。
- 更新 `tests/integration/pipeline_integration_test.cpp`：确保 Runtime 主链路消费 `RequestContext`。

### 最小接口要求
- `RequestContext` 至少包含 method、target/path、version、host、headers 视图、body 视图、client endpoint、trace context 预留字段。
- 解析结果必须区分成功、需要更多数据、协议错误、资源上限错误。
- HTTP/1.1 codec 不得向业务层泄露 `llhttp` 类型。
- 业务层不得直接读取原始 request-line 作为路由依据。

### 验收测试
- 合法 GET 请求解析成功，并输出统一 `RequestContext`。
- 缺失 Host 返回 400。
- 非法 request-line 返回 400。
- Header 数量或大小超限返回 400/431，并有测试覆盖。
- Content-Length 不一致返回 400。
- Runtime 集成测试继续覆盖 200、404、400、405 与 `/metrics`。

## 14.3 C-ROUTE：替换临时路由
命中 MUST：`12.4 路由与限流`、`12.6 热重载与内存模型`。

### 代码落点
- `include/netp2/routing/`：定义只读 `RouteSnapshot`、`RouteMatch` 与匹配接口。
- `src/routing/`：实现 Host + Path(Trie LPM) + Client IP 的匹配顺序。
- `src/runtime/spine.cpp`：删除临时 `route_request`，改为读取已发布快照。

### 验收测试
- Host 命中但 Path 未命中返回 404。
- Path LPM 能选择最长前缀。
- Client IP 规则优先级可验证。
- 路由读取路径不加写锁，不引入跨 Worker 共享可变状态。

## 14.4 C-RESILIENCE：限流与断路器
命中 MUST：`12.4 路由与限流`、`12.5 Upstream 与韧性`、`12.7 可观测与门禁`。

### 代码落点
- `include/netp2/resilience/`：定义限流决策、断路器状态与探活回写接口。
- `src/resilience/`：实现 thread_local 配额切分限流与 Closed/Open/Half-Open 状态机。
- `src/observability/`：补充 429、503、断路器状态与分路由指标。

### 验收测试
- 超限请求返回 429，并记录分路由拒绝指标。
- 断路器 Closed -> Open -> Half-Open -> Closed/Open 状态迁移有单元测试。
- 限流热路径不得依赖全局 mutex 或跨线程共享计数器。

## 14.5 C-UPSTREAM：核心本地连接池
命中 MUST：`12.3 内存与协议栈`、`12.5 Upstream 与韧性`。

### 代码落点
- `include/netp2/upstream/`：定义连接池借还、健康检查、建连超时接口。
- `src/upstream/`：实现按 Worker/Core 本地化的 Keep-Alive 池，按 `Host:Port` 分组。
- `src/runtime/spine.cpp`：将 mock upstream 替换为 upstream client 的最小异步调用边界。

### 验收测试
- 同一 Worker 内连接可复用。
- 复用前健康检查失败时销毁并重建。
- 建连超时返回 504。
- upstream 全部不可用返回 503。

## 14.6 C-CONFIG：RCU 热重载
命中 MUST：`12.6 热重载与内存模型`、`12.8 工程与治理`。

### 代码落点
- `include/netp2/config/`：定义配置快照、发布句柄与读取句柄。
- `src/config/`：实现“新快照构建 -> 按核投递 -> store-release 发布 -> load-acquire 读取 -> 旧版本自然回收”。
- `src/routing/`、`src/resilience/`、`src/upstream/`：只读取不可变快照，不在热路径修改配置对象。

### 验收测试
- 热重载期间请求可继续处理。
- 旧快照在仍被协程持有时不会提前释放。
- race 测试覆盖并发读取与切换。
- PR 中必须写明 release/acquire 的 happens-before 链。

## 14.7 C-QUALITY：验收命令与完成标准
每个 C-* 任务完成时至少执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

涉及内存生命周期、缓冲池、热重载或连接池时，还必须执行对应 sanitizer/race 构建。若本地平台不支持某项 sanitizer，需要在 PR 中说明原因与替代证据。

Phase C 完成标准：
- `RuntimeSpine` 不再包含 HTTP request-line 手写解析。
- `src/protocol/` 成为 HTTP/1.1 解析主入口，HTTP/2 入口具备清晰适配边界。
- 路由、限流、断路器、upstream、配置读取均通过只读快照或核心本地状态接入。
- 热路径没有新增跨线程 `std::mutex` 共享临界区。
- PR 描述包含 “MUST 条款 -> 代码位置 -> 验证命令/测试证据”。