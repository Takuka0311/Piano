# M7 实施记录（导出 + 可观测 + GUI 稳定性收敛）

## 1) 规格冻结与接口收敛
- 新增 `spec-006`，明确导出契约、可观测字段与 CI 分层。
- 抽离后端工厂：
  - `include/piano/app/audio_backend_factory.h`
  - `src/app/audio_backend_factory.cpp`
- `playback_service` 与 `gui_main` 的实时键盘输出改为共用工厂创建后端，消除双份构建逻辑。

## 2) WAV 导出最小链路
- 新增导出模块：
  - `include/piano/export/wav_exporter.h`
  - `src/export/wav_exporter.cpp`
- CLI 新增：
  - `--export-wav <path>`（导出后退出）
- GUI 新增：
  - 导出路径输入框、浏览按钮、`Export WAV` 按钮。
- 配置持久化新增字段：
  - `export_wav_path`

## 3) 可观测与诊断
- 播放快照新增指标：
  - `active_backend`
  - `recoveries`
  - `errors`
  - `emitted_events`
- 播放过程中增加结构化日志行（`[obs] event=... detail=...`）。
- GUI 新增诊断展示行，实时展示后端与关键计数。

## 4) GUI 稳定性修补
- 数值输入解析增加防御（采样率/缓冲区非法输入回退默认值）。
- 导出路径浏览改用保存对话框（可新建文件，不要求文件已存在）。
- 重新布局控制区域，避免导出控件与诊断区冲突。

## 5) 测试与 CI
- 单测增强：
  - `export_wav_path` round-trip
  - WAV 导出可复现测试（同输入双次导出字节一致）
- 新增脚本：
  - `scripts/smoke-export.ps1`
- CI 与本地 CI：
  - 默认快速链路加入导出冒烟步骤。

## 6) 自检结果
- Debug 构建：通过
- Release 构建：通过
- `ctest`：通过
- CLI 导出冒烟：通过（生成 `dist/demo-m7.wav`）
