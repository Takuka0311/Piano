# SPEC-005 UI Alpha（M4）

- 状态：`implemented`
- 优先级：P1

## 背景与目标
- 当前版本以 CLI 为主，缺少可直接操作的桌面界面。
- M4 目标是在不破坏现有核心链路与 CI 的前提下，提供最小可用 GUI。

## 范围
- In Scope：
  - 主窗口与播放/停止控制。
  - `*.keyboard` 与 `*.in` 文件选择。
  - `wasapi/log` 后端切换与参数配置。
  - 键盘可视化（按 MIDI 范围展示 active 状态）。
  - 配置持久化（路径、后端、采样率、缓冲）。
- Out of Scope：
  - 主题美化与复杂编辑器功能。
  - MIDI/VSTi 新能力扩展。

## 功能需求
- GUI 可启动并可执行完整播放闭环。
- 配置可保存并在下次启动恢复。
- 运行时若后端异常，应用可提示并继续工作（或回退）。

## 非功能需求
- 默认 CI 保持快速，不引入长时发声测试。
- GUI 增加可启动 smoke（短时自动退出）。

## 验收标准
- `piano_gui` 可执行以下流程：选文件 -> 配参数 -> 播放/停止。
- `ctest` 保持通过，CLI 不回归。
- CI 默认链路通过，且包含 GUI 启动 smoke。

## 实现记录
- 主要文件：
  - `src/ui/gui_main.cpp`
  - `src/ui/gui_app.cpp`
  - `include/piano/ui/gui_app.h`
  - `src/platform/config_store.cpp`
  - `include/piano/platform/config_store.h`
  - `src/app/playback_service.cpp`
  - `include/piano/app/playback_service.h`
- 说明：
  - 由于当前环境拉取外部 Dear ImGui 依赖失败，M4 Alpha 先采用 Win32 原生 GUI 实现同等功能闭环。
  - 后续可在网络与依赖条件允许时替换为 Dear ImGui 前端而不改核心服务层。
