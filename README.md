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
- 基准目录：`benchmarks/`
- CI 工作流：`.github/workflows/`

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

## License
MIT
