#include "netp2/observability/metrics.h"

#include <sstream>

namespace netp2::observability {

namespace {
thread_local WorkerMetricsShard* tls_worker_shard = nullptr;

std::uint64_t load_relaxed(const std::atomic<std::uint64_t>& v) {
    return v.load(std::memory_order_relaxed);
}
}  // namespace

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry registry;
    return registry;
}

MetricsRegistry::ShardPtr MetricsRegistry::create_worker_shard() {
    auto shard = std::make_shared<WorkerMetricsShard>();
    std::scoped_lock lock(shards_mu_);
    shards_.push_back(shard);
    return shard;
}

void MetricsRegistry::bind_thread_local_shard(WorkerMetricsShard* shard) {
    tls_worker_shard = shard;
}

void MetricsRegistry::unbind_thread_local_shard() {
    tls_worker_shard = nullptr;
}

void MetricsRegistry::record_accept() {
    if (tls_worker_shard == nullptr) {
        return;
    }
    tls_worker_shard->accepts_total.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_request(std::size_t bytes_in) {
    if (tls_worker_shard == nullptr) {
        return;
    }
    tls_worker_shard->requests_total.fetch_add(1, std::memory_order_relaxed);
    tls_worker_shard->bytes_in_total.fetch_add(static_cast<std::uint64_t>(bytes_in),
                                               std::memory_order_relaxed);
}

void MetricsRegistry::record_response(int status_code, std::size_t bytes_out) {
    if (tls_worker_shard == nullptr) {
        return;
    }

    if (status_code >= 200 && status_code < 300) {
        tls_worker_shard->responses_2xx_total.fetch_add(1, std::memory_order_relaxed);
    } else if (status_code >= 400 && status_code < 500) {
        tls_worker_shard->responses_4xx_total.fetch_add(1, std::memory_order_relaxed);
    } else if (status_code >= 500 && status_code < 600) {
        tls_worker_shard->responses_5xx_total.fetch_add(1, std::memory_order_relaxed);
    }

    tls_worker_shard->bytes_out_total.fetch_add(static_cast<std::uint64_t>(bytes_out),
                                                std::memory_order_relaxed);
}

std::string MetricsRegistry::render_prometheus() const {
    std::uint64_t accepts_total = 0;
    std::uint64_t requests_total = 0;
    std::uint64_t responses_2xx_total = 0;
    std::uint64_t responses_4xx_total = 0;
    std::uint64_t responses_5xx_total = 0;
    std::uint64_t bytes_in_total = 0;
    std::uint64_t bytes_out_total = 0;

    {
        std::scoped_lock lock(shards_mu_);
        for (const auto& weak : shards_) {
            const auto shard = weak.lock();
            if (!shard) {
                continue;
            }
            accepts_total += load_relaxed(shard->accepts_total);
            requests_total += load_relaxed(shard->requests_total);
            responses_2xx_total += load_relaxed(shard->responses_2xx_total);
            responses_4xx_total += load_relaxed(shard->responses_4xx_total);
            responses_5xx_total += load_relaxed(shard->responses_5xx_total);
            bytes_in_total += load_relaxed(shard->bytes_in_total);
            bytes_out_total += load_relaxed(shard->bytes_out_total);
        }
    }

    std::ostringstream out;
    out << "# HELP netp2_accepts_total Total accepted TCP connections.\n";
    out << "# TYPE netp2_accepts_total counter\n";
    out << "netp2_accepts_total " << accepts_total << "\n";
    out << "# HELP netp2_requests_total Total handled HTTP requests.\n";
    out << "# TYPE netp2_requests_total counter\n";
    out << "netp2_requests_total " << requests_total << "\n";
    out << "# HELP netp2_responses_total Total HTTP responses by status class.\n";
    out << "# TYPE netp2_responses_total counter\n";
    out << "netp2_responses_total{code=\"2xx\"} " << responses_2xx_total << "\n";
    out << "netp2_responses_total{code=\"4xx\"} " << responses_4xx_total << "\n";
    out << "netp2_responses_total{code=\"5xx\"} " << responses_5xx_total << "\n";
    out << "# HELP netp2_bytes_in_total Total request bytes read.\n";
    out << "# TYPE netp2_bytes_in_total counter\n";
    out << "netp2_bytes_in_total " << bytes_in_total << "\n";
    out << "# HELP netp2_bytes_out_total Total response bytes written.\n";
    out << "# TYPE netp2_bytes_out_total counter\n";
    out << "netp2_bytes_out_total " << bytes_out_total << "\n";

    return out.str();
}

}  // namespace netp2::observability
