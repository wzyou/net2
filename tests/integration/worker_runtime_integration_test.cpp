#include <array>
#include <chrono>
#include <cstddef>
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
bool single_request(std::uint16_t port) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    asio::io_context io(1);
    tcp::socket client(io);
    client.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));

    const std::string request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    asio::write(client, asio::buffer(request));

    std::string response;
    std::array<char, 1024> chunk{};
    boost::system::error_code read_ec;
    for (;;) {
        const std::size_t n = client.read_some(asio::buffer(chunk), read_ec);
        response.append(chunk.data(), n);
        if (read_ec == asio::error::eof) {
            break;
        }
        if (read_ec) {
            return false;
        }
    }

    return response.find("HTTP/1.1 200 OK") != std::string::npos &&
           response.find("netp2 phase-a") != std::string::npos;
}

bool wait_until_ready(std::uint16_t port) {
    for (int i = 0; i < 80; ++i) {
        if (single_request(port)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

}  // namespace

int main() {
    try {
        for (std::uint16_t port = 24000; port < 24100; ++port) {
            netp2::runtime::SpineConfig config;
            config.listen_address = "127.0.0.1";
            config.listen_port = port;
            config.enable_reuse_port = true;

            try {
                netp2::runtime::GatewayRuntime runtime(config, 2);
                runtime.start();

                if (!wait_until_ready(config.listen_port)) {
                    runtime.stop();
                    continue;
                }

                bool ok = true;
                for (int i = 0; i < 16; ++i) {
                    ok = ok && wait_until_ready(config.listen_port);
                }

                ok = ok && (runtime.total_affinity_violation_count() == 0);

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
