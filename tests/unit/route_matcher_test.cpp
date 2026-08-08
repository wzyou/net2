#include "netp2/routing/route_snapshot.h"

#include <cassert>
#include <iostream>

using namespace netp2::routing;

void test_basic_exact_match() {
    std::cout << "Testing basic exact match...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule;
    rule.host_pattern = "api.example.com";
    rule.path_prefix = "/v1/users";
    rule.target.cluster_name = "users-service";
    rule.priority = 0;
    
    snapshot.add_rule(std::move(rule));
    snapshot.finalize();

    const auto match = snapshot.match("api.example.com", "/v1/users/123", "192.168.1.1");
    assert(match.matched);
    assert(match.target.cluster_name == "users-service");
    assert(match.fail_reason == RouteMatch::FailReason::kNone);

    std::cout << "  PASSED\n";
}

void test_wildcard_host() {
    std::cout << "Testing wildcard host match...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule;
    rule.host_pattern = "*.example.com";
    rule.path_prefix = "/api";
    rule.target.cluster_name = "api-cluster";
    
    snapshot.add_rule(std::move(rule));
    snapshot.finalize();

    auto match1 = snapshot.match("api.example.com", "/api/test", "127.0.0.1");
    assert(match1.matched);

    auto match2 = snapshot.match("web.example.com", "/api/data", "127.0.0.1");
    assert(match2.matched);

    auto match3 = snapshot.match("other.com", "/api/test", "127.0.0.1");
    assert(!match3.matched);
    assert(match3.fail_reason == RouteMatch::FailReason::kHostMismatch);

    std::cout << "  PASSED\n";
}

void test_longest_prefix_match() {
    std::cout << "Testing longest prefix match...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule1;
    rule1.host_pattern = "api.example.com";
    rule1.path_prefix = "/api";
    rule1.target.cluster_name = "api-general";
    rule1.priority = 10;
    
    RouteRule rule2;
    rule2.host_pattern = "api.example.com";
    rule2.path_prefix = "/api/v2/users";
    rule2.target.cluster_name = "users-v2";
    rule2.priority = 5;
    
    snapshot.add_rule(std::move(rule1));
    snapshot.add_rule(std::move(rule2));
    snapshot.finalize();

    // 应该匹配更长的前缀
    auto match1 = snapshot.match("api.example.com", "/api/v2/users/123", "127.0.0.1");
    assert(match1.matched);
    assert(match1.target.cluster_name == "users-v2");

    // 应该匹配较短的前缀
    auto match2 = snapshot.match("api.example.com", "/api/v1/data", "127.0.0.1");
    assert(match2.matched);
    assert(match2.target.cluster_name == "api-general");

    std::cout << "  PASSED\n";
}

void test_client_ip_cidr() {
    std::cout << "Testing client IP CIDR filtering...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule;
    rule.host_pattern = "internal.example.com";
    rule.path_prefix = "/admin";
    rule.target.cluster_name = "admin-service";
    rule.allowed_client_cidrs.push_back("192.168.1.0/24");
    rule.allowed_client_cidrs.push_back("10.0.0.1");
    
    snapshot.add_rule(std::move(rule));
    snapshot.finalize();

    // 应该匹配（在 CIDR 范围内）
    auto match1 = snapshot.match("internal.example.com", "/admin/dashboard", "192.168.1.100");
    assert(match1.matched);

    // 应该匹配（精确 IP）
    auto match2 = snapshot.match("internal.example.com", "/admin/dashboard", "10.0.0.1");
    assert(match2.matched);

    // 应该拒绝（不在范围内）
    auto match3 = snapshot.match("internal.example.com", "/admin/dashboard", "203.0.113.1");
    assert(!match3.matched);
    assert(match3.fail_reason == RouteMatch::FailReason::kClientIpDenied);

    std::cout << "  PASSED\n";
}

void test_path_not_found() {
    std::cout << "Testing path not found...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule;
    rule.host_pattern = "api.example.com";
    rule.path_prefix = "/v1";
    rule.target.cluster_name = "v1-service";
    
    snapshot.add_rule(std::move(rule));
    snapshot.finalize();

    // Host 匹配但 Path 不匹配
    auto match = snapshot.match("api.example.com", "/v2/users", "127.0.0.1");
    assert(!match.matched);
    assert(match.fail_reason == RouteMatch::FailReason::kPathMismatch);

    std::cout << "  PASSED\n";
}

void test_priority_ordering() {
    std::cout << "Testing priority ordering...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule1;
    rule1.host_pattern = "api.example.com";
    rule1.path_prefix = "/api";
    rule1.target.cluster_name = "low-priority";
    rule1.priority = 100;
    
    RouteRule rule2;
    rule2.host_pattern = "api.example.com";
    rule2.path_prefix = "/api";
    rule2.target.cluster_name = "high-priority";
    rule2.priority = 1;
    
    snapshot.add_rule(std::move(rule1));
    snapshot.add_rule(std::move(rule2));
    snapshot.finalize();

    // 应该选择高优先级的规则
    auto match = snapshot.match("api.example.com", "/api/test", "127.0.0.1");
    assert(match.matched);
    assert(match.target.cluster_name == "high-priority");

    std::cout << "  PASSED\n";
}

void test_empty_snapshot() {
    std::cout << "Testing empty snapshot...\n";

    RouteSnapshot snapshot;
    snapshot.finalize();

    auto match = snapshot.match("any.host.com", "/any/path", "127.0.0.1");
    assert(!match.matched);
    assert(match.fail_reason == RouteMatch::FailReason::kPathMismatch);

    std::cout << "  PASSED\n";
}

void test_root_path() {
    std::cout << "Testing root path match...\n";

    RouteSnapshot snapshot;
    
    RouteRule rule;
    rule.host_pattern = "example.com";
    rule.path_prefix = "/";
    rule.target.cluster_name = "root-service";
    
    snapshot.add_rule(std::move(rule));
    snapshot.finalize();

    // "/" 规则只匹配精确的根路径
    auto match1 = snapshot.match("example.com", "/", "127.0.0.1");
    assert(match1.matched);
    assert(match1.target.cluster_name == "root-service");

    // "/" 不作为 fallback，不匹配其他路径
    auto match2 = snapshot.match("example.com", "/any/path", "127.0.0.1");
    assert(!match2.matched);  // "/" 不匹配 "/any/path"

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "=== Route Matcher Unit Tests ===\n\n";
    
    try {
        test_basic_exact_match();
        test_wildcard_host();
        test_longest_prefix_match();
        test_client_ip_cidr();
        test_path_not_found();
        test_priority_ordering();
        test_empty_snapshot();
        test_root_path();
        
        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n=== Test FAILED: " << e.what() << " ===\n";
        return 1;
    }
}
