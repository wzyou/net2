#include "netp2/resilience/rate_limiter.h"

#include <algorithm>
#include <chrono>

namespace netp2::resilience {

RateLimiter::RateLimiter(RateLimitConfig config)
    : config_(std::move(config)),
      tokens_(static_cast<double>(config_.burst_size)),
      last_refill_time_(std::chrono::steady_clock::now()) {}

RateLimitDecision RateLimiter::check_allow(const std::string& /* route_key */) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        now - last_refill_time_);

    // 补充令牌：按速率补充，但不超过突发容量
    const double tokens_to_add = elapsed.count() * config_.requests_per_second;
    tokens_ = std::min(tokens_ + tokens_to_add, static_cast<double>(config_.burst_size));
    last_refill_time_ = now;

    // 检查是否有可用令牌
    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        return RateLimitDecision::kAllow;
    }

    return RateLimitDecision::kReject;
}

void RateLimiter::reset() {
    tokens_ = static_cast<double>(config_.burst_size);
    last_refill_time_ = std::chrono::steady_clock::now();
}

}  // namespace netp2::resilience
