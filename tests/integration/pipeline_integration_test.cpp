#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include "netp2/runtime/gateway_runtime.h"

namespace {
bool send_request(std::uint16_t port, const std::string& request, std::string& response_out) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    asio::io_context io(1);
    tcp::socket client(io);
    client.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

    asio::write(client, asio::buffer(request));

    response_out.clear();
    std::array<char, 4096> chunk{};
    boost::system::error_code read_ec;
    for (;;) {
        const std::size_t n = client.read_some(asio::buffer(chunk), read_ec);
        response_out.append(chunk.data(), n);
        if (read_ec == asio::error::eof) {
            break;
        }
        if (read_ec) {
            return false;
        }
    }

    return true;
}

bool http_get(std::uint16_t port, const std::string& path, std::string& response_out) {
    const std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    return send_request(port, request, response_out);
}

bool wait_until_ready(std::uint16_t port) {
    for (int i = 0; i < 80; ++i) {
        std::string response;
        if (http_get(port, "/", response) &&
            response.find("HTTP/1.1 200 OK") != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

bool contains_all(const std::string& text,
                  const std::string& must_have_1,
                  const std::string& must_have_2) {
    return text.find(must_have_1) != std::string::npos &&
           text.find(must_have_2) != std::string::npos;
}
}  // namespace

int main() {
    try {
        for (std::uint16_t port = 24200; port < 24300; ++port) {
            netp2::runtime::SpineConfig config;
            config.listen_address = "127.0.0.1";
            config.listen_port = port;
            config.enable_reuse_port = true;

            try {
                netp2::runtime::GatewayRuntime runtime(config, 2);
                runtime.start();

                if (!wait_until_ready(port)) {
                    runtime.stop();
                    continue;
                }

                std::string proxy_response;
                if (!http_get(port, "/proxy/mock", proxy_response)) {
                    runtime.stop();
                    return 1;
                }

                std::string miss_response;
                if (!http_get(port, "/missing", miss_response)) {
                    runtime.stop();
                    return 1;
                }

                std::string malformed_response;
                const std::string malformed_request = "BROKEN\r\n\r\n";
                if (!send_request(port, malformed_request, malformed_response)) {
                    runtime.stop();
                    return 1;
                }

                const bool ok =
                    contains_all(proxy_response, "HTTP/1.1 200 OK", "mock upstream ok") &&
                    contains_all(miss_response, "HTTP/1.1 404 Not Found", "route not found") &&
                    // 解析错误应该返回 4xx 错误
                    (malformed_response.find("HTTP/1.1 4") != std::string::npos) &&
                    runtime.total_affinity_violation_count() == 0;

                runtime.stop();
                return ok ? 0 : 1;
            } catch (...) {
                continue;
            }
        }

        return 1;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 1;
    } catch (...) {
        return 1;
    }
}
