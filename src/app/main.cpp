#include <csignal>
#include <exception>
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include "netp2/core/version.h"
#include "netp2/runtime/spine.h"

int main() {
    namespace asio = boost::asio;

    try {
        asio::io_context io(1);
        netp2::runtime::SpineConfig config;
        config.listen_address = "0.0.0.0";
        config.listen_port = 18080;

        netp2::runtime::RuntimeSpine spine(io, config);
        spine.start();

        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            spine.stop();
            io.stop();
        });

        std::cout << "netp2 gateway phase-a " << netp2::core::kVersion
                  << " listening on " << config.listen_address << ":" << config.listen_port
                  << std::endl;

        io.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << std::endl;
        return 1;
    }
}
