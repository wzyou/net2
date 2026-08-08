#include "netp2/runtime/gateway_runtime.h"

#include <stdexcept>
#include <utility>

namespace netp2::runtime {

GatewayRuntime::GatewayRuntime(const SpineConfig& config, std::size_t worker_count)
    : config_(config) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker_count must be greater than 0");
    }

    if (worker_count > 1 && config_.listen_port == 0) {
        throw std::invalid_argument("listen_port must be fixed when worker_count > 1");
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        Worker worker;
        worker.io = std::make_unique<boost::asio::io_context>(1);
        worker.spine = std::make_unique<RuntimeSpine>(*worker.io, config_);
        workers_.push_back(std::move(worker));
    }
}

GatewayRuntime::~GatewayRuntime() {
    stop();
}

void GatewayRuntime::start() {
    if (started_) {
        return;
    }

    for (auto& worker : workers_) {
        worker.spine->start();
        worker.thread = std::thread([io = worker.io.get()]() {
            io->run();
        });
    }

    started_ = true;
}

void GatewayRuntime::stop() {
    if (!started_) {
        return;
    }

    for (auto& worker : workers_) {
        worker.spine->stop();
    }

    for (auto& worker : workers_) {
        worker.io->stop();
    }

    for (auto& worker : workers_) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }

    started_ = false;
}

std::size_t GatewayRuntime::worker_count() const {
    return workers_.size();
}

std::uint16_t GatewayRuntime::local_port() const {
    if (workers_.empty()) {
        return 0;
    }
    return workers_.front().spine->local_port();
}

}  // namespace netp2::runtime
