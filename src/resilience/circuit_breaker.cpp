#include "netp2/resilience/circuit_breaker.h"

#include <chrono>

namespace netp2::resilience {

CircuitBreaker::CircuitBreaker(CircuitBreakerConfig config)
    : config_(std::move(config)) {}

bool CircuitBreaker::allow_request() {
    if (state_ == CircuitState::kClosed) {
        return true;
    }

    if (state_ == CircuitState::kOpen) {
        // 检查是否可以进入半开状态
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - open_start_time_);

        if (elapsed >= config_.open_timeout) {
            transition_to_half_open();
            return true;  // 允许探活请求
        }

        return false;  // 仍在 Open 状态，拒绝请求
    }

    // HalfOpen 状态，允许探活请求
    return true;
}

void CircuitBreaker::record_success() {
    if (state_ == CircuitState::kClosed) {
        // Closed 状态下成功不影响状态
        consecutive_failures_ = 0;
        return;
    }

    if (state_ == CircuitState::kHalfOpen) {
        consecutive_successes_++;
        if (consecutive_successes_ >= config_.success_threshold) {
            transition_to_closed();
        }
    }
}

void CircuitBreaker::record_failure() {
    if (state_ == CircuitState::kOpen) {
        // Open 状态下失败不改变状态
        return;
    }

    if (state_ == CircuitState::kClosed) {
        consecutive_failures_++;
        if (consecutive_failures_ >= config_.failure_threshold) {
            transition_to_open();
        }
        return;
    }

    if (state_ == CircuitState::kHalfOpen) {
        // HalfOpen 状态下失败立即回到 Open
        transition_to_open();
    }
}

void CircuitBreaker::transition_to_open() {
    state_ = CircuitState::kOpen;
    open_start_time_ = std::chrono::steady_clock::now();
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
}

void CircuitBreaker::transition_to_half_open() {
    state_ = CircuitState::kHalfOpen;
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
}

void CircuitBreaker::transition_to_closed() {
    state_ = CircuitState::kClosed;
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
}

void CircuitBreaker::reset() {
    state_ = CircuitState::kClosed;
    consecutive_failures_ = 0;
    consecutive_successes_ = 0;
}

}  // namespace netp2::resilience
