#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace netp2::observability {

struct WorkerMetricsShard {
    std::atomic<std::uint64_t> accepts_total{0};
    std::atomic<std::uint64_t> requests_total{0};
    std::atomic<std::uint64_t> responses_2xx_total{0};
    std::atomic<std::uint64_t> responses_4xx_total{0};
    std::atomic<std::uint64_t> responses_5xx_total{0};
    std::atomic<std::uint64_t> bytes_in_total{0};
    std::atomic<std::uint64_t> bytes_out_total{0};
};

class MetricsRegistry {
public:
    using ShardPtr = std::shared_ptr<WorkerMetricsShard>;

    static MetricsRegistry& instance();

    ShardPtr create_worker_shard();
    void bind_thread_local_shard(WorkerMetricsShard* shard);
    void unbind_thread_local_shard();

    void record_accept();
    void record_request(std::size_t bytes_in);
    void record_response(int status_code, std::size_t bytes_out);

    std::string render_prometheus() const;

private:
    MetricsRegistry() = default;

    mutable std::mutex shards_mu_;
    std::vector<std::weak_ptr<WorkerMetricsShard>> shards_;
};

}  // namespace netp2::observability
