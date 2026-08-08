#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "netp2/config/config_snapshot.h"
#include "netp2/protocol/http1_codec.h"
#include "netp2/routing/route_snapshot.h"

namespace netp2::runtime {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct SpineConfig {
    std::string listen_address{"127.0.0.1"};
    std::uint16_t listen_port{18080};
    bool enable_reuse_port{false};
    bool enable_executor_affinity_checks{true};
};

class RuntimeSpine {
public:
    RuntimeSpine(asio::io_context& io, const SpineConfig& config);

    void start();
    void stop();

    /// 热重载路由配置（RCU 语义）
    /// @param snapshot 新的路由快照
    void reload_route_config(std::shared_ptr<routing::RouteSnapshot> snapshot);

    std::uint16_t local_port() const;
    bool is_open() const;
    std::uint64_t affinity_violation_count() const;

private:
    asio::awaitable<void> accept_loop();
    asio::awaitable<void> handle_connection(tcp::socket socket);
    bool check_socket_executor_affinity(tcp::socket& socket);
    asio::awaitable<bool> check_coroutine_executor_affinity();

    asio::io_context& io_;
    tcp::acceptor acceptor_;
    SpineConfig config_;
    std::atomic<std::uint64_t> affinity_violation_count_{0};
    
    // RCU 配置快照管理器
    config::ConfigSnapshotManager<routing::RouteSnapshot> route_config_;
};

}  // namespace netp2::runtime
