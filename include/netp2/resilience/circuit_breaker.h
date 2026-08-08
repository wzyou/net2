#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace netp2::resilience {

/// 断路器状态
enum class CircuitState {
    kClosed,     // 正常状态，允许请求
    kOpen,       // 断开状态，拒绝请求
    kHalfOpen,   // 半开状态，探活请求
};

/// 断路器配置（只读快照）
struct CircuitBreakerConfig {
    std::uint32_t failure_threshold = 5;            // 失败阈值（连续失败次数）
    std::uint32_t success_threshold = 2;            // 成功阈值（半开时连续成功次数）
    std::chrono::seconds open_timeout{30};          // Open 状态持续时间
};

/// Thread-local 断路器（按核心本地化）
class CircuitBreaker {
public:
    explicit CircuitBreaker(CircuitBreakerConfig config);

    /// 检查是否允许请求通过
    /// @return true 允许，false 拒绝
    bool allow_request();

    /// 记录成功调用
    void record_success();

    /// 记录失败调用
    void record_failure();

    /// 获取当前状态
    CircuitState state() const { return state_; }

    /// 重置断路器状态（用于测试）
    void reset();

private:
    void transition_to_open();
    void transition_to_half_open();
    void transition_to_closed();

    CircuitBreakerConfig config_;
    CircuitState state_ = CircuitState::kClosed;

    std::uint32_t consecutive_failures_ = 0;
    std::uint32_t consecutive_successes_ = 0;
    std::chrono::steady_clock::time_point open_start_time_;
};

}  // namespace netp2::resilience
