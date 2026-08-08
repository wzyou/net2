#include "netp2/routing/route_snapshot.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace netp2::routing {

namespace {

/// 简单的 Path Trie 节点
struct TrieNode {
    std::map<std::string, std::unique_ptr<TrieNode>> children;
    std::vector<const RouteRule*> rules;  ///< 该节点关联的规则（按优先级排序）
};

/// 检查 IP 是否在 CIDR 范围内（简化版，仅支持 IPv4）
bool ip_in_cidr(std::string_view ip, std::string_view cidr) {
    // 简化实现：仅做前缀匹配
    // 完整实现应该使用 inet_pton + 掩码计算
    if (cidr.find('/') == std::string_view::npos) {
        return ip == cidr;  // 精确匹配
    }

    const auto slash_pos = cidr.find('/');
    const auto prefix = cidr.substr(0, slash_pos);
    const auto prefix_len_str = cidr.substr(slash_pos + 1);

    // 简化：支持 /24、/16、/8
    // 例如：192.168.1.0/24 匹配 192.168.1.x
    if (prefix_len_str == "24") {
        // 匹配前三段
        const auto prefix_parts = prefix.substr(0, prefix.rfind('.'));
        const auto ip_parts = ip.substr(0, ip.rfind('.'));
        return prefix_parts == ip_parts;
    }

    if (prefix_len_str == "16") {
        // 匹配前两段
        auto count = 0;
        auto pos = std::string_view::npos;
        for (size_t i = 0; i < prefix.size() && count < 2; ++i) {
            if (prefix[i] == '.') {
                ++count;
                if (count == 2) {
                    pos = i;
                    break;
                }
            }
        }
        if (pos != std::string_view::npos) {
            return ip.substr(0, pos) == prefix.substr(0, pos);
        }
    }

    if (prefix_len_str == "8") {
        // 匹配第一段
        const auto prefix_first = prefix.substr(0, prefix.find('.'));
        const auto ip_first = ip.substr(0, ip.find('.'));
        return prefix_first == ip_first;
    }

    if (prefix_len_str == "0") {
        return true;  // 匹配所有
    }

    // 不支持的前缀长度，回退到精确匹配
    return ip == prefix;
}

/// 检查 Host 是否匹配模式（支持通配符 *.example.com）
bool host_matches(std::string_view host, std::string_view pattern) {
    if (pattern == "*") {
        return true;  // 匹配所有
    }

    if (pattern.starts_with("*.")) {
        // 通配符匹配：*.example.com
        const auto suffix = pattern.substr(2);
        return host.ends_with(suffix);
    }

    // 精确匹配
    return host == pattern;
}

/// 将路径切分为段
std::vector<std::string> split_path(std::string_view path) {
    std::vector<std::string> segments;
    std::size_t start = 0;

    if (path.empty() || path[0] != '/') {
        return segments;
    }

    while (start < path.size()) {
        if (path[start] == '/') {
            ++start;
            continue;
        }

        auto end = path.find('/', start);
        if (end == std::string_view::npos) {
            end = path.size();
        }

        segments.emplace_back(path.substr(start, end - start));
        start = end;
    }

    return segments;
}

}  // namespace

// RouteSnapshot::Impl 实现
class RouteSnapshot::Impl {
public:
    void add_rule(RouteRule rule) {
        rules_.push_back(std::move(rule));
    }

    void finalize() {
        // 构建 Trie
        root_ = std::make_unique<TrieNode>();

        for (const auto& rule : rules_) {
            // 特殊处理根路径 "/"
            if (rule.path_prefix == "/") {
                // 根路径规则直接放在 root 节点
                root_->rules.push_back(&rule);
                continue;
            }

            const auto segments = split_path(rule.path_prefix);
            auto* node = root_.get();

            for (const auto& seg : segments) {
                if (!node->children.contains(seg)) {
                    node->children[seg] = std::make_unique<TrieNode>();
                }
                node = node->children[seg].get();
            }

            node->rules.push_back(&rule);
        }

        // 对每个节点的规则按优先级排序
        sort_rules(root_.get());
    }

    RouteMatch match(std::string_view host,
                    std::string_view path,
                    std::string_view client_ip) const {
        RouteMatch result;

        // 找到所有可能的规则（最长前缀匹配）
        const auto candidates = find_longest_prefix_rules(path);

        // 按 Host -> Path -> Client IP 顺序过滤
        for (const auto* rule : candidates) {
            // 1. Host 匹配
            if (!host_matches(host, rule->host_pattern)) {
                result.fail_reason = RouteMatch::FailReason::kHostMismatch;
                continue;
            }

            // 2. Client IP 检查（如果配置了限制）
            if (!rule->allowed_client_cidrs.empty()) {
                bool ip_allowed = false;
                for (const auto& cidr : rule->allowed_client_cidrs) {
                    if (ip_in_cidr(client_ip, cidr)) {
                        ip_allowed = true;
                        break;
                    }
                }
                if (!ip_allowed) {
                    result.fail_reason = RouteMatch::FailReason::kClientIpDenied;
                    continue;
                }
            }

            // 匹配成功
            result.matched = true;
            result.target = rule->target;
            result.matched_rule_id = rule->host_pattern + rule->path_prefix;
            result.fail_reason = RouteMatch::FailReason::kNone;
            return result;
        }

        // 未找到匹配规则
        if (result.fail_reason == RouteMatch::FailReason::kNone) {
            result.fail_reason = RouteMatch::FailReason::kPathMismatch;
        }
        return result;
    }

    std::size_t rule_count() const {
        return rules_.size();
    }

private:
    void sort_rules(TrieNode* node) {
        if (!node) {
            return;
        }

        // 按优先级排序（数字越小优先级越高）
        std::sort(node->rules.begin(), node->rules.end(),
                 [](const RouteRule* a, const RouteRule* b) {
                     return a->priority < b->priority;
                 });

        for (auto& [_, child] : node->children) {
            sort_rules(child.get());
        }
    }

    std::vector<const RouteRule*> find_longest_prefix_rules(std::string_view path) const {
        if (!root_) {
            return {};
        }

        const auto segments = split_path(path);
        
        // 如果是根路径 "/"，直接返回 root 的规则
        if (segments.empty()) {
            return root_->rules;
        }

        auto* node = root_.get();
        const TrieNode* longest_match_node = nullptr;

        // Trie 最长前缀匹配：找到最深的匹配节点
        for (const auto& seg : segments) {
            // 尝试向下匹配
            auto it = node->children.find(std::string(seg));
            if (it == node->children.end()) {
                // 找不到子节点，停止匹配
                break;
            }
            node = it->second.get();

            // 如果当前节点有规则，记录为候选
            if (!node->rules.empty()) {
                longest_match_node = node;
            }
        }

        // 返回最长匹配节点的规则（如果有）
        // "/" 规则不作为 fallback，只匹配精确的 "/"
        if (longest_match_node) {
            return longest_match_node->rules;
        }

        return {};
    }

    std::vector<RouteRule> rules_;
    std::unique_ptr<TrieNode> root_;
};

// RouteSnapshot 公开接口实现
RouteSnapshot::RouteSnapshot() : impl_(std::make_unique<Impl>()) {}

RouteSnapshot::~RouteSnapshot() = default;

RouteSnapshot::RouteSnapshot(RouteSnapshot&&) noexcept = default;
RouteSnapshot& RouteSnapshot::operator=(RouteSnapshot&&) noexcept = default;

void RouteSnapshot::add_rule(RouteRule rule) {
    impl_->add_rule(std::move(rule));
}

void RouteSnapshot::finalize() {
    impl_->finalize();
}

RouteMatch RouteSnapshot::match(std::string_view host,
                                std::string_view path,
                                std::string_view client_ip) const {
    return impl_->match(host, path, client_ip);
}

std::size_t RouteSnapshot::rule_count() const {
    return impl_->rule_count();
}

}  // namespace netp2::routing
