# Piano (重构进行中)

这是一个正在重构的 Windows 钢琴项目仓库。  
当前阶段以文档先行（spec-driven）为主，代码实现尚未开始。

## 当前状态
- 已完成旧版本分析文档整理。
- 已完成重构技术选型与目标架构文档。
- 已建立首批规格文档（spec-001 ~ spec-004）。
- 仓库已完成历史无关文件清理，仅保留文档与配置基础。

## 目录结构
- `docs/legacy`：旧版本实现、功能、风险与可复用资产分析。
- `docs/rebuild`：重构方案、架构设计、构建发布与迁移计划。
- `docs/specs`：spec-driven 规范与首批功能规格。

## 建议阅读顺序
1. `docs/legacy/overview.md`
2. `docs/rebuild/tech-selection.md`
3. `docs/rebuild/target-architecture.md`
4. `docs/specs/README.md`
5. `docs/specs/spec-001-keyboard-and-note-mapping.md`

## 后续开发方向（下一阶段）
- 初始化工程骨架（Windows + CMake + Ninja）。
- 优先落地 spec-001 与 spec-003（键位映射 + 曲谱回放）。
- 建立最小可运行链路：输入 -> 事件 -> 音频输出。
- 补充 CI（构建、打包、发布可运行 exe）。

## 说明
- 这是阶段性 README，完整版本将在核心功能开发完成后补充。
