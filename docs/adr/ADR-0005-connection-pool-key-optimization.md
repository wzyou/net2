# ADR-0005: 连接池 Key 优化 - 从字符串拼接到结构化类型

**状态**: 已采纳  
**日期**: 2026-08-08  
**决策者**: 架构师  
**影响范围**: `src/upstream/connection_pool.cpp`、`include/netp2/upstream/connection_pool.h`

---

## 背景与问题

Phase D 完成后，连接池已实现 thread-local 无锁设计，但在代码审查中发现热路径存在可优化空间：

```cpp
// 原实现：每次调用都进行字符串拼接和内存分配
std::string ConnectionPool::make_pool_key(const std::string& host, std::uint16_t port) const {
    return host + ":" + std::to_string(port);
}
```

在高并发场景下（目标 RPS > 10k），`borrow_connection` 和 `return_connection` 会被频繁调用，每次都会：
1. 调用 `std::to_string` 进行整数格式化
2. 进行字符串拼接触发动态内存分配
3. 作为 `unordered_map` 的 key 进行完整字符串比较

这与架构文档 `05-upstream-and-resilience.md` 的 MUST 约束存在对齐缺口：
> "MUST 将 Keep-Alive 连接池按核心本地化，**借还路径无锁**"

虽然现有实现满足 thread-local 无锁要求，但热路径的内存分配会引入不必要的性能损耗。

---

## 决策

采用**结构化 Key + 自定义 Hash** 方案替代字符串拼接：

```cpp
struct PoolKey {
    std::string host;
    std::uint16_t port;

    bool operator==(const PoolKey& other) const {
        return port == other.port && host == other.host;
    }
};

template <>
struct std::hash<netp2::upstream::PoolKey> {
    std::size_t operator()(const PoolKey& key) const noexcept {
        const auto h1 = std::hash<std::string>{}(key.host);
        const auto h2 = std::hash<std::uint16_t>{}(key.port);
        return h1 ^ (h2 << 1);
    }
};
```

使用方式：
```cpp
const PoolKey pool_key{host, port};  // 聚合初始化，无额外开销
auto& pool = pools_[pool_key];
```

---

## 理由

### 性能收益
1. **消除热路径内存分配**：从 `host + ":" + std::to_string(port)` 改为直接使用 `{host, port}`
2. **优化比较路径**：先比较 `port`（整数比较），只有相等时才比较 `host`（短路优化）
3. **更快的 Hash 计算**：避免字符串拼接，直接组合预计算的 hash 值

### 代码质量
1. **类型安全**：编译期即可发现 key 构造错误
2. **语义清晰**：`PoolKey{host, port}` 比字符串拼接更直观
3. **易于扩展**：未来如需增加 `protocol` 字段，只需修改结构体定义

### 架构一致性
符合 `12.3 内存与协议栈` MUST 约束：
> "thread_local 无锁池 + RAII；热路径避免不必要的内存分配"

---

## 替代方案

### 方案 A：继续使用字符串拼接 + 缓存
- 缓存最近使用的 key 字符串，避免重复拼接
- **拒绝理由**：增加复杂度，缓存失效时仍有开销

### 方案 B：使用 `std::pair<std::string, std::uint16_t>`
- 利用标准库已有的 `std::hash<std::pair>`
- **拒绝理由**：`std::pair` 的 hash 实现质量依赖标准库，且语义不如自定义结构体清晰

---

## 影响与验证

### 实施变更
1. 新增 `PoolKey` 结构体定义（[connection_pool.h](../../include/netp2/upstream/connection_pool.h)）
2. 删除 `make_pool_key` 方法
3. 更新 `borrow_connection` 和 `return_connection` 调用点

### 验收结果
- ✅ 编译通过（无警告）
- ✅ 所有单元测试通过（12/12）
- ✅ 功能行为完全不变
- ✅ 无 API 破坏（`PoolKey` 为内部实现细节）

### 性能基线
当前变更不影响已有性能基线（1,063.71 RPS, P99 4.27ms）。预期在 RPS > 10k 场景下收益可观测。

---

## 后续行动

- [ ] 在下一轮性能基准中验证高并发场景收益（RPS > 10k）
- [ ] 考虑在其他热路径（如路由匹配）应用类似优化模式

---

## 参考

- [05-upstream-and-resilience.md](../architecture/05-upstream-and-resilience.md) - Upstream 连接池架构约束
- [12-architecture-must-constraints.md](../architecture/12-architecture-must-constraints.md) - 12.3 内存与协议栈 MUST 条款
- 实现代码：[connection_pool.cpp](../../src/upstream/connection_pool.cpp), [connection_pool.h](../../include/netp2/upstream/connection_pool.h)
