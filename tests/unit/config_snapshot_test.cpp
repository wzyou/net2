#include "netp2/config/config_snapshot.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace netp2::config;

// 测试配置结构
struct TestConfig {
    int version = 0;
    std::string name;
    
    explicit TestConfig(int v, std::string n = "") 
        : version(v), name(std::move(n)) {}
};

void test_basic_publish_load() {
    std::cout << "Testing basic publish/load...\n";

    ConfigSnapshotManager<TestConfig> manager;

    // 发布配置 v1
    auto config_v1 = std::make_shared<TestConfig>(1, "v1");
    manager.publish(config_v1);

    // 读取配置
    auto loaded = manager.load();
    assert(loaded != nullptr);
    assert(loaded->version == 1);
    assert(loaded->name == "v1");

    std::cout << "  PASSED\n";
}

void test_snapshot_update() {
    std::cout << "Testing snapshot update...\n";

    ConfigSnapshotManager<TestConfig> manager;

    // 发布 v1
    auto config_v1 = std::make_shared<TestConfig>(1, "v1");
    manager.publish(config_v1);

    // 读取 v1
    auto reader1 = manager.load();
    assert(reader1->version == 1);

    // 发布 v2
    auto config_v2 = std::make_shared<TestConfig>(2, "v2");
    manager.publish(config_v2);

    // 旧读者仍然持有 v1
    assert(reader1->version == 1);

    // 新读者获得 v2
    auto reader2 = manager.load();
    assert(reader2->version == 2);

    std::cout << "  PASSED\n";
}

void test_old_version_lifecycle() {
    std::cout << "Testing old version automatic cleanup...\n";

    ConfigSnapshotManager<TestConfig> manager;

    std::weak_ptr<TestConfig> weak_v1;
    std::weak_ptr<TestConfig> weak_v2;

    {
        // 发布 v1
        auto config_v1 = std::make_shared<TestConfig>(1, "v1");
        weak_v1 = config_v1;
        manager.publish(config_v1);

        // 持有 v1
        auto reader_v1 = manager.load();
        assert(!weak_v1.expired());

        // 发布 v2
        auto config_v2 = std::make_shared<TestConfig>(2, "v2");
        weak_v2 = config_v2;
        manager.publish(config_v2);

        // v1 仍然存活（reader_v1 持有）
        assert(!weak_v1.expired());
        assert(!weak_v2.expired());

        // reader_v1 销毁
    }

    // v1 应该被回收（没有人持有）
    // 但 v2 仍然被 manager 持有
    assert(weak_v1.expired());
    assert(!weak_v2.expired());

    std::cout << "  PASSED\n";
}

void test_concurrent_readers() {
    std::cout << "Testing concurrent readers...\n";

    ConfigSnapshotManager<TestConfig> manager;

    // 初始配置
    auto config_v1 = std::make_shared<TestConfig>(1, "v1");
    manager.publish(config_v1);

    const int num_readers = 10;
    std::vector<std::thread> readers;
    std::atomic<int> successful_reads{0};

    // 启动多个读者线程
    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&manager, &successful_reads]() {
            for (int j = 0; j < 100; ++j) {
                auto config = manager.load();
                if (config && config->version >= 1) {
                    ++successful_reads;
                }
            }
        });
    }

    // 在后台持续更新配置
    std::thread publisher([&manager]() {
        for (int v = 2; v <= 10; ++v) {
            auto new_config = std::make_shared<TestConfig>(v, "v" + std::to_string(v));
            manager.publish(new_config);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 等待所有线程完成
    for (auto& t : readers) {
        t.join();
    }
    publisher.join();

    // 验证所有读取都成功
    assert(successful_reads == num_readers * 100);

    std::cout << "  PASSED (readers: " << num_readers 
              << ", reads: " << successful_reads << ")\n";
}

void test_happens_before_guarantee() {
    std::cout << "Testing happens-before guarantee...\n";

    struct DataConfig {
        std::vector<int> data;
    };

    ConfigSnapshotManager<DataConfig> manager;

    // 发布方：构建配置并发布
    auto config = std::make_shared<DataConfig>();
    config->data = {1, 2, 3, 4, 5};  // Step 1: 写入数据
    manager.publish(config);          // Step 2: store-release

    // 读取方：应该能看到所有写入
    auto loaded = manager.load();     // Step 3: load-acquire
    
    // Step 1 的所有写入对 Step 3 之后可见
    assert(loaded->data.size() == 5);
    assert(loaded->data[0] == 1);
    assert(loaded->data[4] == 5);

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "=== Config Snapshot RCU Unit Tests ===\n\n";

    try {
        test_basic_publish_load();
        test_snapshot_update();
        test_old_version_lifecycle();
        test_concurrent_readers();
        test_happens_before_guarantee();

        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}
