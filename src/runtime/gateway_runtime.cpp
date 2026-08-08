#include "netp2/runtime/gateway_runtime.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sstream>
#endif

namespace netp2::runtime {

namespace {

#if defined(__linux__)
void bind_thread_to_core_or_throw(std::thread& worker_thread, std::size_t core_index) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(static_cast<int>(core_index), &cpu_set);

    const int rc = pthread_setaffinity_np(
        worker_thread.native_handle(),
        sizeof(cpu_set_t),
        &cpu_set);
    if (rc != 0) {
        std::ostringstream oss;
        oss << "pthread_setaffinity_np failed for core " << core_index << ": "
            << std::strerror(rc);
        throw std::runtime_error(oss.str());
    }
}
#endif

}  // namespace

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

    try {
#if defined(__linux__)
        const auto detected_cores = static_cast<std::size_t>(std::thread::hardware_concurrency());
        const auto hardware_cores = detected_cores == 0 ? 1 : detected_cores;
        if (workers_.size() > hardware_cores) {
            throw std::invalid_argument("worker_count exceeds available CPU cores on Linux");
        }
#endif

        for (std::size_t i = 0; i < workers_.size(); ++i) {
            auto& worker = workers_[i];
            worker.spine->start();
            worker.thread = std::thread([io = worker.io.get()]() {
                io->run();
            });

#if defined(__linux__)
            bind_thread_to_core_or_throw(worker.thread, i);
#endif
        }
    } catch (...) {
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
        throw;
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

std::uint64_t GatewayRuntime::total_affinity_violation_count() const {
    std::uint64_t total = 0;
    for (const auto& worker : workers_) {
        total += worker.spine->affinity_violation_count();
    }
    return total;
}

}  // namespace netp2::runtime
