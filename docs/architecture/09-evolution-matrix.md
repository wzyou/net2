# 09. 演进路线矩阵

| 能力模块 | v0.4 | v0.5 | v0.6 | v0.7 | v0.8 | v0.9/v1.0 |
|---|---|---|---|---|---|---|
| 运行架构 | TPC + Reuseport | TPC | TPC | TPC | TPC | TPC + Shared-Nothing |
| 内存池 | thread_local 无锁 | thread_local 无锁 | thread_local 无锁 | thread_local 无锁 | thread_local 无锁 | thread_local 无锁 + RAII |
| 限流 | - | 令牌桶/漏桶 | 令牌桶/漏桶 | 令牌桶/漏桶 | 令牌桶/漏桶 | 配额切分 + 无锁限流 |
| 协议解析 | - | - | nghttp2/llhttp | nghttp2/llhttp | nghttp2/llhttp | HTTP/2 卸载 + HTTP/1.1 |
| 路由引擎 | - | - | 三维 Trie | 三维 Trie | 三维 Trie | Host + Path + IP |
| Upstream 池 | - | - | Thread-Local 池 | Thread-Local 池 | Thread-Local 池 | Thread-Local Keep-Alive |
| 热重载 | - | - | - | RCU 指针交换 | RCU 指针交换 | RCU + UDS 管理接口 |
| 高可用 | - | - | - | - | 三态断路 + 探活 | 三态断路 + 协程探活 |
| 可观测 | - | - | - | - | - | Prometheus + OTel |

## 版本治理建议
- 每次能力升级在 PR 中声明“架构影响面”和“回滚路径”。
- 对运行时模型、热重载、安全策略变更必须补充 ADR。
