#pragma once

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "netp2/runtime/spine.h"

namespace netp2::runtime {

class GatewayRuntime {
public:
    GatewayRuntime(const SpineConfig& config, std::size_t worker_count);
    ~GatewayRuntime();

    void start();
    void stop();

    std::size_t worker_count() const;
    std::uint16_t local_port() const;
    std::uint64_t total_affinity_violation_count() const;

private:
    struct Worker {
        std::unique_ptr<boost::asio::io_context> io;
        std::unique_ptr<RuntimeSpine> spine;
        std::thread thread;
    };

    SpineConfig config_;
    std::vector<Worker> workers_;
    bool started_{false};
};

}  // namespace netp2::runtime
