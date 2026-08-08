#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace netp2::config {

/// RCU（Read-Copy-Update）配置快照管理器
/// 
/// 职责：
/// - 发布新配置快照（独占锁写入）
/// - 读取当前配置快照（共享锁读取）
/// - 旧版本自动回收（std::shared_ptr 引用计数）
///
/// 线程安全性：
/// - 发布操作：独占锁保护（单写者或多写者序列化）
/// - 读取操作：共享锁保护（多读者并发）
/// - 读多写少场景下性能优异
///
/// 内存模型：
/// ```
/// Thread 1 (Publisher):              Thread 2 (Reader):
///   1. new_config = build()
///   2. exclusive_lock.lock()          3. shared_lock.lock()
///   4. current_ = new_config          5. local = current_
///   6. exclusive_lock.unlock()        6. shared_lock.unlock()
///   ────────────────────────────────────────────────────────
///   Happens-before: mutex 保证 Step 1-4 对 Step 5 可见
/// ```
template <typename T>
class ConfigSnapshotManager {
public:
    ConfigSnapshotManager() = default;

    /// 发布新配置快照
    /// @param snapshot 新配置快照（shared_ptr）
    /// 
    /// 线程安全：独占锁保护
    /// - 保证所有构建快照的写入对后续读取可见
    /// - 旧快照在所有读者释放引用后自动回收
    void publish(std::shared_ptr<T> snapshot) {
        std::unique_lock lock(mutex_);
        current_ = std::move(snapshot);
    }

    /// 读取当前配置快照
    /// @return 当前配置快照的共享指针
    ///
    /// 线程安全：共享锁保护
    /// - 多个读者可以并发读取
    /// - 返回的 shared_ptr 延长快照生命周期，直到协程完成
    std::shared_ptr<T> load() const {
        std::shared_lock lock(mutex_);
        return current_;
    }

private:
    mutable std::shared_mutex mutex_;  // 读写锁
    std::shared_ptr<T> current_;       // 当前配置快照
};

}  // namespace netp2::config
