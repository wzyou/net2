#include <array>
#include <cstring>
#include <string>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include "netp2/runtime/spine.h"

int main() {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    asio::io_context server_io(1);
    netp2::runtime::SpineConfig config;
    config.listen_address = "127.0.0.1";
    config.listen_port = 0;

    netp2::runtime::RuntimeSpine spine(server_io, config);
    spine.start();

    std::thread server_thread([&server_io]() {
        server_io.run();
    });

    int exit_code = 0;
    try {
        asio::io_context client_io(1);
        tcp::socket client(client_io);
        client.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), spine.local_port()));

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
                throw boost::system::system_error(read_ec);
            }
        }

        if (response.find("HTTP/1.1 200 OK") == std::string::npos) {
            return 1;
        }
        if (response.find("netp2 phase-a") == std::string::npos) {
            return 1;
        }
    } catch (...) {
        exit_code = 1;
    }

    spine.stop();
    server_io.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    return exit_code;
}
