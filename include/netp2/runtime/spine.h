#pragma once

#include <cstdint>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace netp2::runtime {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct SpineConfig {
    std::string listen_address{"127.0.0.1"};
    std::uint16_t listen_port{18080};
};

class RuntimeSpine {
public:
    RuntimeSpine(asio::io_context& io, const SpineConfig& config);

    void start();
    void stop();

    std::uint16_t local_port() const;

private:
    asio::awaitable<void> accept_loop();
    asio::awaitable<void> handle_connection(tcp::socket socket);

    asio::io_context& io_;
    tcp::acceptor acceptor_;
    SpineConfig config_;
};

}  // namespace netp2::runtime
