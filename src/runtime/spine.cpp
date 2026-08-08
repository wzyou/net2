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

namespace netp2::runtime {

namespace {
constexpr std::string_view kDefaultRoutePath = "/";
constexpr std::string_view kMockUpstreamPath = "/proxy/mock";
constexpr std::string_view kLegacyPhaseABody = "netp2 phase-a\n";
constexpr std::string_view kMockUpstreamBody = "mock upstream ok\n";
constexpr std::string_view kNotFoundBody = "route not found\n";
constexpr std::string_view kBadRequestBody = "bad request\n";
constexpr std::string_view kMethodNotAllowedBody = "method not allowed\n";

constexpr std::string_view kMetricsPath = "/metrics";

struct ParsedRequestLine {
    std::string_view method;
    std::string_view path;
    std::string_view version;
};

enum class RouteDecision {
    kMockUpstream,
    kNotFound,
};

std::optional<ParsedRequestLine> parse_request_line(std::string_view request) {
    const auto line_end = request.find("\r\n");
    if (line_end == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view line = request.substr(0, line_end);
    const auto first_space = line.find(' ');
    if (first_space == std::string_view::npos || first_space == 0) {
        return std::nullopt;
    }

    const auto second_space = line.find(' ', first_space + 1);
    if (second_space == std::string_view::npos || second_space == first_space + 1 ||
        second_space + 1 >= line.size()) {
        return std::nullopt;
    }

    ParsedRequestLine parsed{
        line.substr(0, first_space),
        line.substr(first_space + 1, second_space - first_space - 1),
        line.substr(second_space + 1)};

    if (!parsed.version.starts_with("HTTP/1.")) {
        return std::nullopt;
    }

    return parsed;
}

RouteDecision route_request(std::string_view path) {
    if (path == kDefaultRoutePath || path == kMockUpstreamPath) {
        return RouteDecision::kMockUpstream;
    }
    return RouteDecision::kNotFound;
}

asio::awaitable<std::string_view> mock_upstream_call(std::string_view path) {
    // 主动让出执行权，避免热点连接长期占用同核事件循环。
    co_await asio::post(asio::use_awaitable);
    if (path == kDefaultRoutePath) {
        co_return kLegacyPhaseABody;
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

    std::array<char, 1024> request_buffer{};
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

    const auto parsed = parse_request_line(request_view);
    if (!parsed) {
        response_status_code = 400;
        response_payload = build_http_response(400, "Bad Request", kBadRequestBody);
    } else if (parsed->method != "GET") {
        response_status_code = 405;
        response_payload = build_http_response(405, "Method Not Allowed", kMethodNotAllowedBody);
    } else if (parsed->path == kMetricsPath) {
        const std::string metrics_body =
            netp2::observability::MetricsRegistry::instance().render_prometheus();
        response_payload = build_http_response(
            200,
            "OK",
            metrics_body,
            "text/plain; version=0.0.4; charset=utf-8");
    } else {
        switch (route_request(parsed->path)) {
        case RouteDecision::kMockUpstream: {
            const std::string_view upstream_body = co_await mock_upstream_call(parsed->path);
            response_status_code = 200;
            response_payload = build_http_response(200, "OK", upstream_body);
            break;
        }
        case RouteDecision::kNotFound:
            response_status_code = 404;
            response_payload = build_http_response(404, "Not Found", kNotFoundBody);
            break;
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
