# netp2

高性能 C++20 协程异步网关项目。

## 架构文档入口
- AI 约束与全局护栏：`.github/copilot-instructions.md`
- 模块化架构文档：`docs/architecture/`
- 架构决策记录（ADR）：`docs/adr/`

详细的架构约束与模块边界请参考 `docs/architecture/` 目录下的所有规范文档。

## 文档治理规则
- 架构文档是代码的一部分，代码变更需要同步更新对应文档。
- 任何跨线程共享可变状态的设计变更，必须先更新架构文档并通过审查。
- 重大架构演进（协议、拓扑、配置热更新机制）必须新增 ADR。

## 项目脚手架
- 工程骨架说明：`docs/architecture/11-project-scaffolding.md`
- 构建系统：`CMakeLists.txt` + `cmake/`
- 配置样例：`configs/`
- 测试目录：`tests/`
- 基准目录：`benchmarks/`（性能基准执行说明见 `benchmarks/wrk/README.md`）
- CI 工作流：`.github/workflows/`
- 运维脚本：`scripts/ops/`（部署、启停、健康检查、热重载）

## 本地快速开始
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

或使用脚本：
```bash
./scripts/dev/build.sh
```

## 性能基准
```bash
# 构建 Release 版本
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# 执行性能基准（需先安装 wrk）
cd benchmarks/wrk
./run_baseline.sh --host localhost --port 8080
```

详细基准说明与 CI 门禁配置见 `docs/architecture/16-phase-d-implementation-guide.md`。

## 运维手册
- 部署与启停：`scripts/ops/`
- 配置管理：`configs/gateway.template.yaml`
- 运维手册：`docs/ops/runbook.md`（Phase D 补充）
- 故障排查：`docs/ops/troubleshooting.md`（Phase D 补充）

## License
MIT
