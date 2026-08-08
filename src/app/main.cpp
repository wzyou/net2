#include <algorithm>
#include <csignal>
#include <exception>
#include <iostream>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include "netp2/core/version.h"
#include "netp2/runtime/gateway_runtime.h"

int main() {
    namespace asio = boost::asio;

    try {
        netp2::runtime::SpineConfig config;
        config.listen_address = "0.0.0.0";
        config.listen_port = 18080;
        config.enable_reuse_port = true;

        const std::size_t workers =
            std::max<std::size_t>(1, static_cast<std::size_t>(std::thread::hardware_concurrency()));
        netp2::runtime::GatewayRuntime runtime(config, workers);
        runtime.start();

        asio::io_context signal_io(1);
        asio::signal_set signals(signal_io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            runtime.stop();
            signal_io.stop();
        });

        std::cout << "netp2 gateway phase-b-step1 " << netp2::core::kVersion
                  << " workers=" << workers
                  << " listening on " << config.listen_address << ":" << config.listen_port
                  << std::endl;

        signal_io.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fatal: " << ex.what() << std::endl;
        return 1;
    }
}
