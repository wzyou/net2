#include "netp2/resilience/circuit_breaker.h"

#include <cassert>
#include <iostream>
#include <thread>

using namespace netp2::resilience;

void test_closed_to_open() {
    std::cout << "Testing Closed -> Open transition...\n";

    CircuitBreakerConfig config;
    config.failure_threshold = 3;
    config.success_threshold = 2;
    config.open_timeout = std::chrono::seconds(1);

    CircuitBreaker breaker(config);

    assert(breaker.state() == CircuitState::kClosed);
    assert(breaker.allow_request());

    breaker.record_failure();
    assert(breaker.state() == CircuitState::kClosed);
    breaker.record_failure();
    assert(breaker.state() == CircuitState::kClosed);
    breaker.record_failure();
    assert(breaker.state() == CircuitState::kOpen);
    assert(!breaker.allow_request());

    std::cout << "  PASSED\n";
}

void test_open_to_half_open() {
    std::cout << "Testing Open -> HalfOpen transition...\n";

    CircuitBreakerConfig config;
    config.failure_threshold = 2;
    config.open_timeout = std::chrono::seconds(1);

    CircuitBreaker breaker(config);

    breaker.record_failure();
    breaker.record_failure();
    assert(breaker.state() == CircuitState::kOpen);

    std::this_thread::sleep_for(std::chrono::seconds(1) + std::chrono::milliseconds(50));

    assert(breaker.allow_request());
    assert(breaker.state() == CircuitState::kHalfOpen);

    std::cout << "  PASSED\n";
}

void test_half_open_to_closed() {
    std::cout << "Testing HalfOpen -> Closed transition...\n";

    CircuitBreakerConfig config;
    config.failure_threshold = 2;
    config.success_threshold = 2;
    config.open_timeout = std::chrono::seconds(1);

    CircuitBreaker breaker(config);

    breaker.record_failure();
    breaker.record_failure();
    std::this_thread::sleep_for(std::chrono::seconds(1) + std::chrono::milliseconds(50));
    breaker.allow_request();

    assert(breaker.state() == CircuitState::kHalfOpen);

    breaker.record_success();
    assert(breaker.state() == CircuitState::kHalfOpen);
    breaker.record_success();
    assert(breaker.state() == CircuitState::kClosed);

    std::cout << "  PASSED\n";
}

void test_half_open_to_open() {
    std::cout << "Testing HalfOpen -> Open (failure) transition...\n";

    CircuitBreakerConfig config;
    config.failure_threshold = 2;
    config.open_timeout = std::chrono::seconds(1);

    CircuitBreaker breaker(config);

    breaker.record_failure();
    breaker.record_failure();
    std::this_thread::sleep_for(std::chrono::seconds(1) + std::chrono::milliseconds(50));
    breaker.allow_request();

    assert(breaker.state() == CircuitState::kHalfOpen);

    breaker.record_failure();
    assert(breaker.state() == CircuitState::kOpen);
    assert(!breaker.allow_request());

    std::cout << "  PASSED\n";
}

void test_success_resets_failure_count() {
    std::cout << "Testing success resets failure count in Closed state...\n";

    CircuitBreakerConfig config;
    config.failure_threshold = 3;

    CircuitBreaker breaker(config);

    breaker.record_failure();
    breaker.record_failure();
    assert(breaker.state() == CircuitState::kClosed);

    breaker.record_success();

    breaker.record_failure();
    breaker.record_failure();
    assert(breaker.state() == CircuitState::kClosed);

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "=== Circuit Breaker Unit Tests ===\n\n";

    try {
        test_closed_to_open();
        test_open_to_half_open();
        test_half_open_to_closed();
        test_half_open_to_open();
        test_success_resets_failure_count();

        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (...) {
        return 1;
    }
}
