#include "netp2/runtime/spine.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

#include <sys/socket.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include "netp2/observability/metrics.h"
#include "netp2/protocol/request_context.h"
#include "netp2/routing/route_snapshot.h"

namespace netp2::runtime {

namespace {
constexpr std::string_view kLegacyPhaseABody = "netp2 phase-a\n";
constexpr std::string_view kMockUpstreamBody = "mock upstream ok\n";
constexpr std::string_view kNotFoundBody = "route not found\n";
constexpr std::string_view kMethodNotAllowedBody = "method not allowed\n";

constexpr std::string_view kMetricsPath = "/metrics";

asio::awaitable<std::string_view> mock_upstream_call(std::string_view path) {
    // 主动让出执行权，避免热点连接长期占用同核事件循环。
    co_await asio::post(asio::use_awaitable);
    // 统一返回 mock 响应
    if (path == "/" || path.starts_with("/proxy")) {
        co_return path == "/" ? kLegacyPhaseABody : kMockUpstreamBody;
    }
    co_return kMockUpstreamBody;
}

std::string build_http_response(int status_code,
                                std::string_view reason,
                                std::string_view body,
                                std::string_view content_type = "text/plain; charset=utf-8") {
    std::ostringstream out;
    out << "HTTP/1.1 " << status_code << ' ' << reason << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "\r\n";
    out << body;
    return out.str();
}
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

    // 创建并发布默认路由快照
    auto default_snapshot = std::make_shared<routing::RouteSnapshot>();
    
    // 配置特定路径
    routing::RouteRule rule_proxy;
    rule_proxy.host_pattern = "*";
    rule_proxy.path_prefix = "/proxy/mock";
    rule_proxy.target.cluster_name = "mock-upstream";
    rule_proxy.priority = 10;
    
    routing::RouteRule rule_root;
    rule_root.host_pattern = "*";
    rule_root.path_prefix = "/";  // 精确匹配根路径
    rule_root.target.cluster_name = "root-service";
    rule_root.priority = 100;
    
    default_snapshot->add_rule(std::move(rule_proxy));
    default_snapshot->add_rule(std::move(rule_root));
    default_snapshot->finalize();
    
    route_config_.publish(std::move(default_snapshot));
}

void RuntimeSpine::reload_route_config(std::shared_ptr<routing::RouteSnapshot> snapshot) {
    route_config_.publish(std::move(snapshot));
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

        netp2::observability::MetricsRegistry::instance().record_accept();

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

    // 每个连接创建独立的 codec 实例，避免并发冲突
    protocol::Http1Codec codec;

    std::array<char, 8192> request_buffer{};
    boost::system::error_code read_ec;

    const std::size_t request_bytes = co_await socket.async_read_some(
        asio::buffer(request_buffer),
        asio::redirect_error(asio::use_awaitable, read_ec));

    if (read_ec && read_ec != asio::error::eof) {
        co_return;
    }

    netp2::observability::MetricsRegistry::instance().record_request(request_bytes);

    const std::string_view request_view(request_buffer.data(), request_bytes);
    std::string response_payload;
    int response_status_code = 200;

    // 使用 Http1Codec 解析请求
    protocol::RequestContext ctx;
    const auto parse_result = codec.parse(request_view, ctx);

    if (parse_result != protocol::ParseError::kSuccess) {
        // 解析失败，返回对应的错误响应
        response_status_code = protocol::parse_error_to_http_status(parse_result);
        const std::string error_desc(protocol::parse_error_to_string(parse_result));
        response_payload = build_http_response(
            response_status_code,
            "Bad Request",
            error_desc);
    } else if (ctx.method != "GET") {
        // 当前只支持 GET 方法
        response_status_code = 405;
        response_payload = build_http_response(405, "Method Not Allowed", kMethodNotAllowedBody);
    } else if (ctx.target == kMetricsPath) {
        // 指标导出端点
        const std::string metrics_body =
            netp2::observability::MetricsRegistry::instance().render_prometheus();
        response_payload = build_http_response(
            200,
            "OK",
            metrics_body,
            "text/plain; version=0.0.4; charset=utf-8");
    } else {
        // 加载当前路由快照（RCU read）
        auto route_snapshot = route_config_.load();

        // 使用路由快照进行匹配
        const auto route_match = route_snapshot->match(
            ctx.host,
            ctx.target,
            "127.0.0.1");  // TODO: 从 socket 获取真实 client IP

        if (route_match.matched) {
            // 路由匹配成功，调用 mock upstream
            const std::string_view upstream_body = co_await mock_upstream_call(ctx.target);
            response_status_code = 200;
            response_payload = build_http_response(200, "OK", upstream_body);
        } else {
            // 路由未匹配
            response_status_code = 404;
            response_payload = build_http_response(404, "Not Found", kNotFoundBody);
        }
    }

    co_await asio::post(io_.get_executor(), asio::use_awaitable);

    if (config_.enable_executor_affinity_checks &&
        !(co_await check_coroutine_executor_affinity())) {
        co_return;
    }

    boost::system::error_code write_ec;
    co_await asio::async_write(
        socket,
        asio::buffer(response_payload),
        asio::redirect_error(asio::use_awaitable, write_ec));

    if (!write_ec) {
        netp2::observability::MetricsRegistry::instance().record_response(
            response_status_code,
            response_payload.size());
    }

    boost::system::error_code shutdown_ec;
    [[maybe_unused]] const auto shutdown_result =
        socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
    if (shutdown_ec) {
        co_return;
    }
}

}  // namespace netp2::runtime
