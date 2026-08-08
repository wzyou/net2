#include "netp2/upstream/connection_pool.h"

#include <chrono>
#include <utility>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

namespace netp2::upstream {

UpstreamConnection::UpstreamConnection(boost::asio::ip::tcp::socket socket,
                                       std::chrono::steady_clock::time_point create_time)
    : socket_(std::move(socket)),
      create_time_(create_time),
      last_used_time_(create_time) {}

ConnectionHealth UpstreamConnection::check_health(std::chrono::milliseconds idle_timeout) const {
    const auto now = std::chrono::steady_clock::now();
    const auto idle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_used_time_);

    // 检查空闲超时
    if (idle_duration > idle_timeout) {
        return ConnectionHealth::kUnhealthy;
    }

    // 检查 socket 是否仍然打开
    if (!socket_.is_open()) {
        return ConnectionHealth::kUnhealthy;
    }

    return ConnectionHealth::kHealthy;
}

void UpstreamConnection::mark_used() {
    last_used_time_ = std::chrono::steady_clock::now();
}

ConnectionPool::ConnectionPool(boost::asio::io_context& io, UpstreamConfig config)
    : io_(io), config_(std::move(config)) {}

boost::asio::awaitable<std::pair<std::unique_ptr<UpstreamConnection>, UpstreamError>>
ConnectionPool::borrow_connection(const std::string& host, std::uint16_t port) {
    const PoolKey pool_key{host, port};
    auto& pool = pools_[pool_key];

    // 尝试从池中获取健康的连接
    while (!pool.empty()) {
        auto conn = std::move(pool.back());
        pool.pop_back();

        // 健康检查
        if (conn->check_health(config_.idle_timeout) == ConnectionHealth::kHealthy) {
            conn->mark_used();
            co_return std::make_pair(std::move(conn), UpstreamError::kSuccess);
        }

        // 不健康的连接会被自动销毁（RAII）
    }

    // 池中没有可用连接，创建新连接
    auto [new_conn, error] = co_await create_new_connection(host, port);
    co_return std::make_pair(std::move(new_conn), error);
}

void ConnectionPool::return_connection(const std::string& host, std::uint16_t port,
                                       std::unique_ptr<UpstreamConnection> conn) {
    if (!conn) {
        return;
    }

    // 健康检查
    if (conn->check_health(config_.idle_timeout) != ConnectionHealth::kHealthy) {
        return;  // 不健康的连接直接销毁
    }

    const PoolKey pool_key{host, port};
    auto& pool = pools_[pool_key];

    // 检查池是否已满
    if (pool.size() >= config_.max_connections_per_host) {
        return;  // 超过上限，销毁连接
    }

    pool.push_back(std::move(conn));
}

void ConnectionPool::cleanup_idle_connections() {
    for (auto& [key, pool] : pools_) {
        pool.erase(
            std::remove_if(pool.begin(), pool.end(),
                          [this](const auto& conn) {
                              return conn->check_health(config_.idle_timeout) !=
                                     ConnectionHealth::kHealthy;
                          }),
            pool.end());
    }
}

boost::asio::awaitable<std::pair<std::unique_ptr<UpstreamConnection>, UpstreamError>>
ConnectionPool::create_new_connection(const std::string& host, std::uint16_t port) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    try {
        // 解析地址
        tcp::resolver resolver(io_);
        auto endpoints = co_await resolver.async_resolve(
            host, std::to_string(port), asio::use_awaitable);

        // 创建 socket 并设置超时
        tcp::socket socket(io_);
        
        // 简化实现：直接使用 async_connect，不实现复杂的超时机制
        // 在生产环境中，可以使用 socket 级别的超时选项
        co_await socket.async_connect(*endpoints.begin(), asio::use_awaitable);

        // 连接成功
        const auto now = std::chrono::steady_clock::now();
        auto conn = std::make_unique<UpstreamConnection>(std::move(socket), now);
        co_return std::make_pair(std::move(conn), UpstreamError::kSuccess);

    } catch (const boost::system::system_error& ex) {
        // 根据错误类型判断
        if (ex.code() == asio::error::timed_out ||
            ex.code() == asio::error::operation_aborted) {
            co_return std::make_pair(nullptr, UpstreamError::kConnectTimeout);
        }
        co_return std::make_pair(nullptr, UpstreamError::kAllUnavailable);
    } catch (...) {
        co_return std::make_pair(nullptr, UpstreamError::kAllUnavailable);
    }
}

}  // namespace netp2::upstream
