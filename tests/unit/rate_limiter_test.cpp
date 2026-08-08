#include "netp2/resilience/rate_limiter.h"

#include <cassert>
#include <iostream>
#include <thread>

using namespace netp2::resilience;

void test_basic_rate_limit() {
    std::cout << "Testing basic rate limiting...\n";

    RateLimitConfig config;
    config.requests_per_second = 10;  // 每秒 10 个请求
    config.burst_size = 5;            // 突发容量 5

    RateLimiter limiter(config);

    // 前 5 个请求应该允许（突发容量）
    for (int i = 0; i < 5; ++i) {
        assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    }

    // 第 6 个请求应该被拒绝（令牌耗尽）
    assert(limiter.check_allow("test") == RateLimitDecision::kReject);

    std::cout << "  PASSED\n";
}

void test_token_refill() {
    std::cout << "Testing token refill...\n";

    RateLimitConfig config;
    config.requests_per_second = 10;  // 每秒 10 个请求
    config.burst_size = 5;

    RateLimiter limiter(config);

    // 消耗所有令牌
    for (int i = 0; i < 5; ++i) {
        limiter.check_allow("test");
    }

    // 等待 200ms（应该补充 2 个令牌）
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 应该允许 2 个请求
    assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    assert(limiter.check_allow("test") == RateLimitDecision::kReject);

    std::cout << "  PASSED\n";
}

void test_burst_capacity() {
    std::cout << "Testing burst capacity limit...\n";

    RateLimitConfig config;
    config.requests_per_second = 10;
    config.burst_size = 3;

    RateLimiter limiter(config);

    // 等待足够长时间（确保令牌补充到上限）
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 只能使用突发容量的令牌（3 个），不会无限累积
    assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    assert(limiter.check_allow("test") == RateLimitDecision::kAllow);
    assert(limiter.check_allow("test") == RateLimitDecision::kReject);

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "=== Rate Limiter Unit Tests ===\n\n";

    try {
        test_basic_rate_limit();
        test_token_refill();
        test_burst_capacity();

        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (...) {
        return 1;
    }
}
