#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace netp2::upstream {

/// 连接池 Key（避免热路径字符串拼接）
struct PoolKey {
    std::string host;
    std::uint16_t port;

    bool operator==(const PoolKey& other) const {
        return port == other.port && host == other.host;
    }
};

}  // namespace netp2::upstream

/// PoolKey 的 Hash 函数
template <>
struct std::hash<netp2::upstream::PoolKey> {
    std::size_t operator()(const netp2::upstream::PoolKey& key) const noexcept {
        // 组合 hash：host 的 hash 与 port 异或
        const auto h1 = std::hash<std::string>{}(key.host);
        const auto h2 = std::hash<std::uint16_t>{}(key.port);
        return h1 ^ (h2 << 1);  // 简单但有效的组合
    }
};

namespace netp2::upstream {

/// 上游连接配置
struct UpstreamConfig {
    std::chrono::milliseconds connect_timeout{5000};   // 建连超时
    std::chrono::milliseconds idle_timeout{60000};     // 空闲超时
    std::uint32_t max_connections_per_host = 100;      // 每个 Host 最大连接数
};

/// 连接健康状态
enum class ConnectionHealth {
    kHealthy,       // 健康
    kUnhealthy,     // 不健康（需要销毁）
};

/// 上游连接错误
enum class UpstreamError {
    kSuccess,
    kConnectTimeout,      // 建连超时 -> 504
    kAllUnavailable,      // 所有连接不可用 -> 503
    kPoolExhausted,       // 连接池耗尽 -> 503
};

/// 上游连接（RAII wrapper）
class UpstreamConnection {
public:
    UpstreamConnection(boost::asio::ip::tcp::socket socket, 
                       std::chrono::steady_clock::time_point create_time);

    ~UpstreamConnection() = default;

    // 禁止拷贝，允许移动
    UpstreamConnection(const UpstreamConnection&) = delete;
    UpstreamConnection& operator=(const UpstreamConnection&) = delete;
    UpstreamConnection(UpstreamConnection&&) = default;
    UpstreamConnection& operator=(UpstreamConnection&&) = default;

    /// 获取底层 socket
    boost::asio::ip::tcp::socket& socket() { return socket_; }

    /// 检查连接健康状态
    ConnectionHealth check_health(std::chrono::milliseconds idle_timeout) const;

    /// 标记连接为已使用
    void mark_used();

private:
    boost::asio::ip::tcp::socket socket_;
    std::chrono::steady_clock::time_point create_time_;
    std::chrono::steady_clock::time_point last_used_time_;
};

/// Thread-local 上游连接池（按 Host:Port 分组）
class ConnectionPool {
public:
    explicit ConnectionPool(boost::asio::io_context& io, UpstreamConfig config = {});

    /// 借用连接（如果没有可用连接则建立新连接）
    /// @param host 目标主机
    /// @param port 目标端口
    /// @return 连接和错误码
    boost::asio::awaitable<std::pair<std::unique_ptr<UpstreamConnection>, UpstreamError>>
    borrow_connection(const std::string& host, std::uint16_t port);

    /// 归还连接（健康的连接会被复用）
    /// @param host 目标主机
    /// @param port 目标端口
    /// @param conn 连接
    void return_connection(const std::string& host, std::uint16_t port,
                          std::unique_ptr<UpstreamConnection> conn);

    /// 清理空闲超时连接
    void cleanup_idle_connections();

private:
    boost::asio::awaitable<std::pair<std::unique_ptr<UpstreamConnection>, UpstreamError>>
    create_new_connection(const std::string& host, std::uint16_t port);

    boost::asio::io_context& io_;
    UpstreamConfig config_;

    // 按 Host:Port 分组的连接池（使用结构化 Key 避免字符串拼接）
    std::unordered_map<PoolKey, std::vector<std::unique_ptr<UpstreamConnection>>> pools_;
};

}  // namespace netp2::upstream
