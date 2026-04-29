# spec-006 导出与可观测（M7）

- 状态：implemented
- 版本：v1.0
- 关联里程碑：M7

## 背景与目标
- 在现有播放链路基础上补齐“可交付”能力：离线 WAV 导出。
- 在 CLI/GUI 路径补齐“可观测”能力：关键状态、恢复次数、错误计数与事件规模可追踪。

## 范围

### In Scope
- CLI：新增 `--export-wav <path>`，导出并退出。
- GUI：新增导出路径输入、浏览与导出按钮。
- 新增离线导出模块：固定输入下输出可复现。
- 运行态观测：后端名、恢复次数、错误计数、事件计数。
- CI 快速分层：默认流程加入导出冒烟。

### Out of Scope
- VST3 与复杂插件管理。
- 高级音频母带处理、压缩器、限幅器。

## 输入输出契约
- 输入：
  - `keyboard_map_path`
  - `score_path`
  - `sample_rate`
  - `output_wav_path`
- 输出：
  - PCM16 双声道 WAV 文件（RIFF/WAVE 标准头）
- 可复现判定：
  - 相同输入与采样率下，导出文件字节级一致。

## 功能需求
1. CLI 支持 `--export-wav`，成功返回 `0` 并输出导出路径。
2. GUI 支持一键导出，失败时弹窗，成功后状态栏显示结果。
3. 播放链路与实时键盘共用后端工厂，避免重复实现。
4. `piano-ui-config.ini` 持久化 `export_wav_path`。
5. 播放服务快照包含：
   - `active_backend`
   - `recoveries`
   - `errors`
   - `emitted_events`

## 非功能需求
- 默认 CI 不增加设备依赖。
- 导出逻辑可在无音频设备环境运行。
- 观测日志不引入第三方依赖，先用文本结构化行。

## 验收标准
- `piano_tests` 包含导出可复现用例并通过。
- `scripts/smoke-export.ps1` 在 Release 可执行并产生 WAV。
- GUI 诊断区可看到后端与计数变化。

## 测试计划
- 单测：
  - 配置 round-trip 包含 `export_wav_path`
  - 同输入双次导出文件一致
- 冒烟：
  - CLI：`--export-wav`
  - GUI：启动 + 导出按钮
- CI：
  - 默认流程加入导出冒烟
  - MIDI/VSTi 仍保留手动慢测

## 风险与回滚
- 风险：离线导出音色与实时后端不同（属于预期，M7 只保证最小可用和可复现）。
- 回滚：可仅回滚导出入口和脚本，不影响实时播放主链路。
