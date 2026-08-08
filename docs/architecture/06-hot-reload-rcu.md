# 06. 无锁热重载（RCU 风格）

## 6.1 目标
- 配置变更零停机、零全局锁阻塞、零连接中断。
- 变更对象：路由、限流配额、Upstream 节点集。

## 6.2 指针交换模型
1. 管理面生成新快照 `RouteTable V2`。
2. 通过 `asio::post` 投递至各 Core。
3. Worker 以原子发布替换本地指针。
4. 旧协程继续持有 `V1` 的 `shared_ptr` 直到自然结束。
5. `V1` 引用归零后自动回收。

## 6.3 内存序约束
- 发布：store-release。
- 读取：load-acquire。
- 必须可证明 happens-before 链。

## 6.4 安全要点
- 禁止裸指针跨协程长期持有。
- 禁止在回收条件不明时提前释放旧快照。
- 若引入 epoch/hazard pointer，必须文档化回收边界。

## 6.5 管理接口
- 基础入口：Unix Domain Socket（本地最小暴露面）。
- 未来扩展：可由 sidecar 对接 xDS/etcd，再转推 UDS。

## 6.6 MUST 约束核对项
- MUST 使用“新快照构建 -> 按核投递 -> 原子切换 -> 旧版本自然回收”流程。
- MUST 使用 store-release 发布、load-acquire 读取，并给出 happens-before 证明。
- MUST 在协程作用域内以 `shared_ptr` 或等价强引用持有快照。
- MUST 禁止裸指针跨协程长期持有配置对象。
- MUST 在回收边界未证明前禁止提前释放旧快照。
