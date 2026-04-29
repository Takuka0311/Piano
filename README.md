# Piano (Windows 重构版)

这是一个仅面向 Windows 的钢琴项目重构仓库，采用 spec-driven 开发方式。

## 当前里程碑状态（M1 已落地）
- 已完成最小可运行 CLI 骨架（`CMake + Ninja + C++20`）。
- 已完成模块拆分：`input / score / engine / audio / app`。
- 已实现 spec-001 + spec-003 的最小闭环：
  - 读取 `*.keyboard` 键位映射；
  - 读取 `*.in` 曲谱并调度；
  - 打通 `输入 -> 事件 -> 输出`（当前输出为日志占位 `LogOutput`）。
- 已提供本地脚本：`configure/build/run-demo`。

## 目录结构
- `include/piano`：对外头文件（模块接口）。
- `src`：模块实现。
- `assets`：示例键位与示例曲谱。
- `scripts`：本地构建与运行脚本。
- `docs/legacy`：旧版本分析。
- `docs/rebuild`：重构方案与迁移文档。
- `docs/specs`：规格与状态管理。

## 本地构建与运行
在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```

程序会输出调度后的事件日志（NoteOn/NoteOff、TempoChange、TransposeChange），用于 M1 兼容与回归验证。

## 下一步（M2）
- 在 `audio` 模块接入 WASAPI 最小真实发声路径。
- 为 spec-001/spec-003 增加单元测试与回归样例。
- 增加 GitHub Actions，产出可运行 exe 压缩包。
