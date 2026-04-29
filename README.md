# Piano (Windows 重构版)

这是一个仅面向 Windows 的钢琴项目重构仓库，采用 spec-driven 开发方式。

## 当前里程碑状态（M4 Alpha 已落地）
- 已完成最小可运行 CLI 骨架（`CMake + Ninja + C++20`）。
- 已完成模块拆分：`input / score / engine / audio / app`。
- 已实现 spec-001 + spec-003 闭环：
  - 读取 `*.keyboard` 键位映射；
  - 读取 `*.in` 曲谱并调度；
  - 打通 `输入 -> 事件 -> 输出`（支持 `log` 与 `wasapi` 后端）。
- 已将 WASAPI 升级为持续混音链路，支持 NoteOn/NoteOff 与复音。
- 已支持运行时健康检查，WASAPI 异常时可回退 `log`。
- 已新增 GUI Alpha（`piano_gui`）：播放控制、文件选择、参数配置、键盘可视化、配置持久化。
- 已增加基础自动化测试（CTest）与回归样例。
- 已提供本地脚本：`configure/build/run-demo/package/ci-local-check`。
- 已提供 GitHub Actions：`.github/workflows/windows-ci.yml`。

## 目录结构
- `include/piano`：对外头文件（模块接口）。
- `src`：模块实现。
- `assets`：示例键位与示例曲谱。
- `scripts`：本地构建与运行脚本。
- `docs/guides`：开发、使用与排错指南。
- `docs/legacy`：旧版本分析。
- `docs/rebuild`：重构方案与迁移文档。
- `docs/specs`：规格与状态管理。

## 本地构建与运行
在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend wasapi -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend log -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
.\build\piano_gui.exe
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -BuildType Release -BuildDir build-release -OutputDir dist
```

`wasapi` 模式会按事件触发音符；`log` 模式会输出调度日志（NoteOn/NoteOff、TempoChange、TransposeChange），用于回归验证。

## 使用文档
- `docs/guides/README.md`
- `docs/guides/quick-start.md`
- `docs/guides/code-overview.md`
- `docs/guides/troubleshooting.md`

## 后续路线图
- `docs/roadmap/README.md`
- `docs/roadmap/future-plan.md`

## 下一步（M5）
- 强化设备热切换/断开恢复与 DirectSound 回退能力。
- 接入 MIDI Out / VSTi 增强路径并完善发布说明。
- 持续完善音频质量（包络、失真控制、延迟优化）。
