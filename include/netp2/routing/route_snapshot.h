#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace netp2::routing {

/// 路由目标（upstream cluster）
struct RouteTarget {
    std::string cluster_name;  ///< Upstream cluster 名称
    std::string path_rewrite;  ///< 路径重写规则（可选）
};

/// 路由规则
struct RouteRule {
    /// Host 匹配模式（支持精确匹配和通配符 *.example.com）
    std::string host_pattern;

    /// Path 前缀（用于 Trie 最长前缀匹配）
    std::string path_prefix;

    /// 允许的 Client IP CIDR 列表（空则不限制）
    std::vector<std::string> allowed_client_cidrs;

    /// 路由目标
    RouteTarget target;

    /// 规则优先级（数字越小优先级越高，用于同前缀冲突）
    int priority = 0;
};

/// 路由匹配结果
struct RouteMatch {
    bool matched = false;           ///< 是否匹配成功
    RouteTarget target;             ///< 匹配到的目标
    std::string matched_rule_id;   ///< 匹配到的规则 ID（用于指标分类）

    /// 未匹配原因
    enum class FailReason {
        kNone,
        kHostMismatch,
        kPathMismatch,
        kClientIpDenied,
    } fail_reason = FailReason::kNone;
};

/// 路由快照（只读，线程安全）
/// 由配置加载器生成，通过 RCU 发布到各 Worker
class RouteSnapshot {
public:
    RouteSnapshot();
    ~RouteSnapshot();

    // 禁止拷贝
    RouteSnapshot(const RouteSnapshot&) = delete;
    RouteSnapshot& operator=(const RouteSnapshot&) = delete;

    // 支持移动
    RouteSnapshot(RouteSnapshot&&) noexcept;
    RouteSnapshot& operator=(RouteSnapshot&&) noexcept;

    /// 添加路由规则（构建阶段调用）
    void add_rule(RouteRule rule);

    /// 完成构建，编译内部数据结构（Trie 等）
    void finalize();

    /// 执行路由匹配
    /// @param host Host header 值
    /// @param path 请求路径
    /// @param client_ip 客户端 IP 地址
    /// @return 路由匹配结果
    RouteMatch match(std::string_view host,
                    std::string_view path,
                    std::string_view client_ip) const;

    /// 获取规则数量（用于指标）
    std::size_t rule_count() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace netp2::routing
