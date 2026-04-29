# M4 实施记录：GUI Alpha

## 目标
- 提供最小可用 GUI，覆盖播放控制、文件选择、参数调整、可视化与配置持久化。
- 保持 CLI 与测试链路稳定。

## 已完成
- GUI：
  - 新增 `piano_gui` 目标（Win32 窗口）。
  - 支持：
    - 播放/停止
    - 选择 `*.keyboard` / `*.in`
    - 切换 `wasapi/log`
    - 调整 `sample_rate/buffer_ms`
    - 活动 MIDI 键可视化（48..84）
- 服务层复用：
  - 抽取 `playback_service`，CLI 与 GUI 共享播放逻辑。
- 配置持久化：
  - 新增 `config_store`，保存并恢复 GUI 配置。
- 验证与 CI：
  - 新增 `scripts/run-gui-smoke.ps1`
  - CI 增加 GUI 启动 smoke（短时自动退出）
  - 默认 CI 仍保持快速链路，不含长时发声测试。

## 与原计划差异说明
- 原计划使用 Dear ImGui。
- 当前环境下拉取 ImGui 远程依赖失败（TLS/SSL），因此 M4 Alpha 采用 Win32 原生 GUI 作为离线可落地替代方案。
- 服务层已完成解耦，后续可无缝替换前端实现。

## 本地验证
- Debug/Release 构建通过。
- CTest 通过。
- `run-demo`（log）通过。
- `run-gui-smoke` 通过。
- `ci-local-check` 通过。
