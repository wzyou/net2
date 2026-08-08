#include "netp2/runtime/spine.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>
#include <system_error>

#include <sys/socket.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

namespace netp2::runtime {

namespace {
constexpr std::string_view kPhaseAResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 15\r\n"
    "Connection: close\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "netp2 phase-a\n";
}  // namespace

RuntimeSpine::RuntimeSpine(asio::io_context& io, const SpineConfig& config)
    : io_(io),
      acceptor_(io),
      config_(config) {
    const auto address = asio::ip::make_address(config_.listen_address);
    const tcp::endpoint endpoint(address, config_.listen_port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));

    if (config_.enable_reuse_port) {
#if defined(SO_REUSEPORT)
        const int on = 1;
        if (::setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) !=
            0) {
            throw std::system_error(errno, std::generic_category(), "setsockopt SO_REUSEPORT");
        }
#else
        throw std::runtime_error("SO_REUSEPORT is not supported on this platform");
#endif
    }

    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);
}

void RuntimeSpine::start() {
    asio::co_spawn(io_, accept_loop(), asio::detached);
}

void RuntimeSpine::stop() {
    boost::system::error_code ec;
    [[maybe_unused]] const auto close_result = acceptor_.close(ec);
    if (ec) {
        return;
    }
}

std::uint16_t RuntimeSpine::local_port() const {
    return acceptor_.local_endpoint().port();
}

bool RuntimeSpine::is_open() const {
    return acceptor_.is_open();
}

std::uint64_t RuntimeSpine::affinity_violation_count() const {
    return affinity_violation_count_.load(std::memory_order_relaxed);
}

bool RuntimeSpine::check_socket_executor_affinity(tcp::socket& socket) {
    const auto& expected_ctx = io_.get_executor().context();
    const auto& socket_ctx = socket.get_executor().context();
    if (&socket_ctx != &expected_ctx) {
        affinity_violation_count_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

asio::awaitable<bool> RuntimeSpine::check_coroutine_executor_affinity() {
    const auto current_executor = co_await asio::this_coro::executor;
    const auto& expected_ctx = io_.get_executor().context();
    if (&current_executor.context() != &expected_ctx) {
        affinity_violation_count_.fetch_add(1, std::memory_order_relaxed);
        co_return false;
    }
    co_return true;
}

asio::awaitable<void> RuntimeSpine::accept_loop() {
    for (;;) {
        boost::system::error_code ec;
        tcp::socket socket =
            co_await acceptor_.async_accept(asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            if (ec == asio::error::operation_aborted || !acceptor_.is_open()) {
                co_return;
            }
            continue;
        }

        if (config_.enable_executor_affinity_checks && !check_socket_executor_affinity(socket)) {
            boost::system::error_code ignored_ec;
            [[maybe_unused]] const auto close_result = socket.close(ignored_ec);
            continue;
        }

        // 连接处理协程与 accept 协程同 executor，保证单核闭环的最小形态。
        asio::co_spawn(io_, handle_connection(std::move(socket)), asio::detached);
    }
}

asio::awaitable<void> RuntimeSpine::handle_connection(tcp::socket socket) {
    if (config_.enable_executor_affinity_checks) {
        if (!check_socket_executor_affinity(socket)) {
            co_return;
        }
        if (!(co_await check_coroutine_executor_affinity())) {
            co_return;
        }
    }

    std::array<char, 1024> request_buffer{};
    boost::system::error_code read_ec;

    co_await socket.async_read_some(
        asio::buffer(request_buffer),
        asio::redirect_error(asio::use_awaitable, read_ec));

    if (read_ec && read_ec != asio::error::eof) {
        co_return;
    }

    if (config_.enable_executor_affinity_checks &&
        !(co_await check_coroutine_executor_affinity())) {
        co_return;
    }

    boost::system::error_code write_ec;
    co_await asio::async_write(
        socket,
        asio::buffer(kPhaseAResponse.data(), kPhaseAResponse.size()),
        asio::redirect_error(asio::use_awaitable, write_ec));

    boost::system::error_code shutdown_ec;
    [[maybe_unused]] const auto shutdown_result =
        socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
    if (shutdown_ec) {
        co_return;
    }
}

}  // namespace netp2::runtime
