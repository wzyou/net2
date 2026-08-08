#include "netp2/config/config_snapshot.h"
#include "netp2/routing/route_snapshot.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace netp2;

/// Race Test 1: 并发读取配置快照（验证 shared_lock 正确性）
void test_concurrent_config_reads() {
    std::cout << "Race Test 1: Concurrent config reads...\n";

    config::ConfigSnapshotManager<routing::RouteSnapshot> manager;

    // 发布初始配置
    auto snapshot = std::make_shared<routing::RouteSnapshot>();
    routing::RouteRule rule;
    rule.host_pattern = "*";
    rule.path_prefix = "/test";
    rule.target.cluster_name = "test-cluster";
    snapshot->add_rule(std::move(rule));
    snapshot->finalize();
    manager.publish(snapshot);

    const int num_threads = 20;
    const int reads_per_thread = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> successful_reads{0};

    // 启动多个读者线程
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < reads_per_thread; ++j) {
                auto config = manager.load();
                if (config) {
                    ++successful_reads;
                }
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    assert(successful_reads == num_threads * reads_per_thread);
    std::cout << "  PASSED (threads: " << num_threads 
              << ", reads: " << successful_reads << ")\n";
}

/// Race Test 2: 并发读写（验证 RCU 更新期间读取的安全性）
void test_concurrent_read_write() {
    std::cout << "Race Test 2: Concurrent read/write...\n";

    config::ConfigSnapshotManager<routing::RouteSnapshot> manager;

    // 初始配置
    auto initial = std::make_shared<routing::RouteSnapshot>();
    routing::RouteRule rule;
    rule.host_pattern = "*";
    rule.path_prefix = "/v1";
    rule.target.cluster_name = "v1-cluster";
    initial->add_rule(std::move(rule));
    initial->finalize();
    manager.publish(initial);

    const int num_readers = 10;
    const int num_writers = 2;
    const int operations = 500;

    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    std::vector<std::thread> readers;
    std::vector<std::thread> writers;

    // 启动读者线程
    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                auto config = manager.load();
                if (config) {
                    ++read_count;
                }
                std::this_thread::yield();
            }
        });
    }

    // 启动写者线程
    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back([&]() {
            for (int j = 0; j < operations; ++j) {
                auto new_snapshot = std::make_shared<routing::RouteSnapshot>();
                routing::RouteRule new_rule;
                new_rule.host_pattern = "*";
                new_rule.path_prefix = "/v" + std::to_string(j + 2);
                new_rule.target.cluster_name = "v" + std::to_string(j + 2) + "-cluster";
                new_snapshot->add_rule(std::move(new_rule));
                new_snapshot->finalize();
                
                manager.publish(new_snapshot);
                ++write_count;
                
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // 等待写者完成
    for (auto& t : writers) {
        t.join();
    }

    // 停止读者
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) {
        t.join();
    }

    std::cout << "  PASSED (reads: " << read_count 
              << ", writes: " << write_count << ")\n";
    
    assert(write_count == num_writers * operations);
}

/// Race Test 3: 快照生命周期（验证旧快照不会过早释放）
void test_snapshot_lifecycle_under_load() {
    std::cout << "Race Test 3: Snapshot lifecycle under concurrent load...\n";

    config::ConfigSnapshotManager<routing::RouteSnapshot> manager;

    // 初始配置
    auto initial = std::make_shared<routing::RouteSnapshot>();
    manager.publish(initial);

    const int num_threads = 15;
    const int iterations = 200;
    std::vector<std::thread> threads;
    std::atomic<bool> error_detected{false};

    // 每个线程持有快照一段时间
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < iterations; ++j) {
                // 获取快照
                auto snapshot = manager.load();
                
                if (!snapshot) {
                    error_detected = true;
                    return;
                }

                // 模拟使用快照
                std::this_thread::sleep_for(std::chrono::microseconds(50));

                // 如果是某些线程，发布新快照
                if (i % 3 == 0 && j % 10 == 0) {
                    auto new_snapshot = std::make_shared<routing::RouteSnapshot>();
                    manager.publish(new_snapshot);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    assert(!error_detected);
    std::cout << "  PASSED (threads: " << num_threads << ")\n";
}

/// Race Test 4: 高频更新下的读取一致性
void test_high_frequency_updates() {
    std::cout << "Race Test 4: High-frequency config updates...\n";

    config::ConfigSnapshotManager<routing::RouteSnapshot> manager;

    // 初始配置
    auto initial = std::make_shared<routing::RouteSnapshot>();
    manager.publish(initial);

    const int num_readers = 8;
    const int num_updates = 1000;
    std::atomic<bool> stop{false};
    std::atomic<int> null_reads{0};

    std::vector<std::thread> readers;

    // 读者线程：应该永远不会读到 nullptr
    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&]() {
            while (!stop.load()) {
                auto config = manager.load();
                if (!config) {
                    ++null_reads;
                }
            }
        });
    }

    // 主线程：高频更新配置
    for (int i = 0; i < num_updates; ++i) {
        auto new_snapshot = std::make_shared<routing::RouteSnapshot>();
        manager.publish(new_snapshot);
    }

    stop.store(true);
    for (auto& t : readers) {
        t.join();
    }

    // 验证没有读到空指针
    assert(null_reads == 0);
    std::cout << "  PASSED (updates: " << num_updates 
              << ", null reads: " << null_reads << ")\n";
}

int main() {
    std::cout << "=== Race Condition Tests (C-QUALITY-1) ===\n\n";

    try {
        test_concurrent_config_reads();
        test_concurrent_read_write();
        test_snapshot_lifecycle_under_load();
        test_high_frequency_updates();

        std::cout << "\n=== All race tests PASSED ===\n";
        std::cout << "Run with TSan (build-tsan) to detect data races\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Race test failed: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Race test failed with unknown exception\n";
        return 1;
    }
}
