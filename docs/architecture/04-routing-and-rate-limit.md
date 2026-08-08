# 04. 路由引擎与限流策略

## 4.1 三维静态路由流水线
1. Host 精确/通配匹配（Hash + Wildcard 回退）
2. Path 最长前缀匹配（Trie）
3. Client IP 校验（CIDR/黑白名单）

```mermaid
flowchart LR
    I[Incoming Request] --> H[Host Match]
    H --> P[Path Trie LPM]
    P --> IP[Client IP CIDR Check]
    IP --> T[Target Upstream Cluster]
```

## 4.2 复杂度与数据布局
- Path 匹配复杂度约为 O(k)，k 为路径分段深度。
- 路由规则在启动或热重载阶段编译为只读快照。
- 快照按 Shared-Nothing 复制到各 Core 本地读取。

## 4.3 限流模型：thread_local 配额切分
- 全局配额按核心均分，避免跨线程争用。
- 示例：100000 QPS / 16 核 = 每核 6250 QPS。

## 4.4 算法
- 令牌桶：不足即 429（Direct Reject）。
- 带突发漏桶：允许受控排队；超出最大等待阈值则拒绝。
- 协程排队通过 `steady_timer` 挂起/恢复，不阻塞线程。

## 4.5 指标要求
- 按路由输出通过率、429 比例、排队时延分位。
- 每核独立采样，聚合时统一归并。

## 4.6 MUST 约束核对项
- MUST 按 Host -> Path Trie LPM -> Client IP 的固定顺序完成匹配。
- MUST 将路由规则编译为只读快照，运行时读取不得引入写锁。
- MUST 限流状态按线程本地切分，禁止跨线程共享同一配额状态。
- MUST 在拒绝或排队路径输出可观测结果（429、排队时延、命中规则）。
- MUST 保证匹配复杂度上界可评估并在评审中说明。
