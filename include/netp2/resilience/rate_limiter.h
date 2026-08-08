#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace netp2::resilience {

/// 限流决策
enum class RateLimitDecision {
    kAllow,    // 允许通过
    kReject,   // 超限拒绝
};

/// 限流配置（只读快照）
struct RateLimitConfig {
    std::uint32_t requests_per_second = 1000;  // 每秒允许的请求数
    std::uint32_t burst_size = 100;            // 突发容量
};

/// Thread-local 限流器（按核心配额切分，避免跨线程竞争）
class RateLimiter {
public:
    explicit RateLimiter(RateLimitConfig config);

    /// 检查是否允许请求通过
    /// @param route_key 路由键（用于分路由限流）
    /// @return 限流决策
    RateLimitDecision check_allow(const std::string& route_key);

    /// 重置限流状态（用于测试）
    void reset();

private:
    RateLimitConfig config_;

    // 令牌桶算法：维护当前令牌数与上次补充时间
    double tokens_ = 0.0;
    std::chrono::steady_clock::time_point last_refill_time_;
};

}  // namespace netp2::resilience
