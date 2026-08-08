# Phase C Quality Assurance: ASan/TSan Build Instructions

## 概述
Phase C 完成后，必须通过 Sanitizer 验证内存安全和线程安全。

## ASan/LSan 构建（内存泄漏检测）

```bash
# 配置 ASan 构建
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g"

# 编译
cmake --build build-asan -j

# 运行测试
ctest --test-dir build-asan --output-on-failure
```

## TSan 构建（数据竞争检测）

```bash
# 配置 TSan 构建
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"

# 编译
cmake --build build-tsan -j

# 运行 race 测试
./build-tsan/tests/netp2_config_race_test
```

**注意**：ASan 和 TSan 不能同时启用，必须分别构建。

## 预期结果

### ASan/LSan 应该报告
- ✅ 无内存泄漏
- ✅ 无 use-after-free
- ✅ 无 buffer overflow

### TSan 应该报告
- ✅ 无数据竞争
- ✅ ConfigSnapshotManager 使用 shared_mutex 正确同步
- ✅ 路由快照读取无竞态

## Race 测试场景

1. **并发读取**：20 个线程 × 1000 次读取
2. **并发读写**：10 个读者 + 2 个写者同时操作
3. **快照生命周期**：验证旧快照不会过早释放
4. **高频更新**：1000 次连续更新，读者不应读到空指针

## 验收标准

```bash
# 1. 所有单元测试通过（Debug 构建）
cmake --build build && ctest --test-dir build --output-on-failure

# 2. ASan/LSan 无报错
ctest --test-dir build-asan --output-on-failure

# 3. TSan 无报错（race 测试）
./build-tsan/tests/netp2_config_race_test

# 4. 集成测试通过
./build/tests/netp2_pipeline_integration_test
```

## 已知限制

- macOS ARM64 平台不支持 `std::atomic<std::shared_ptr<T>>`
- 使用 `std::shared_mutex` 替代，提供相同的线程安全保证
- TSan 可能在 macOS 上有 false positive，以 Linux 结果为准

## Phase C 完成检查清单

- [x] C-PROTO-1: HTTP/1.1 parsing with llhttp
- [x] C-ROUTE-1: Host+Path(Trie)+ClientIP routing
- [x] C-RESILIENCE-1: Rate limiter + Circuit breaker
- [x] C-UPSTREAM-1: Connection pool
- [x] C-CONFIG-1: RCU hot reload
- [ ] C-QUALITY-1: Sanitizers + race tests ← **当前任务**

## 故障排查

### ASan 报告内存泄漏
```bash
# 查看详细报告
ASAN_OPTIONS=verbosity=1:log_path=asan.log ./build-asan/tests/netp2_xxx_test
cat asan.log
```

### TSan 报告数据竞争
```bash
# 查看详细报告
TSAN_OPTIONS=verbosity=1:log_path=tsan.log ./build-tsan/tests/netp2_config_race_test
cat tsan.log
```

### 性能测试
```bash
# 验证 hot path 无锁竞争
perf record -g ./build/src/netp2_gateway
perf report
```
