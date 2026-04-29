# M6 实施记录（MIDI Out + VST2.4 最小宿主）

## 范围确认
- 本阶段执行 CLI + GUI 双入口。
- VST 范围限定为 VST2.4 最小宿主，不包含 VST3 与复杂插件管理。

## 主要落地

### 1) 后端与回退链
- 新增 `MidiOutputSink`（WinMM）与 `VstiOutputSink`（VST2.4 最小加载）。
- 播放服务回退链升级为：
  - `vsti -> midiout -> wasapi -> dsound -> log`
- `output_mode=vsti` 时，插件加载失败会自动降级到 `midiout`。

### 2) 双入口接入
- CLI 新增参数：
  - `--output-mode`
  - `--midi-out-device`
  - `--vsti-plugin`
- GUI 新增：
  - 输出模式下拉（含 `midiout/vsti`）
  - MIDI 设备文本输入
  - VSTi DLL 路径输入与浏览按钮

### 3) 配置持久化
- 新增持久化字段：
  - `output_mode`
  - `midi_out_device`
  - `vsti_plugin_path`
- 与原有 `backend_priority/recent_*` 一起统一存储于 `piano-ui-config.ini`。

### 4) 验证与 CI 分层
- 快速链路保持不变（默认 PR 不运行设备/插件慢测）。
- 新增手动验证脚本：
  - `scripts/smoke-midiout.ps1`
  - `scripts/smoke-vsti.ps1`
- GitHub Actions `workflow_dispatch` 新增可选输入：
  - `run_midiout_smoke`
  - `run_vsti_smoke`
  - `vsti_plugin_path`

## 自检结果
- Debug 构建通过。
- Release 构建通过。
- `ctest` 通过。
- CLI `--help` 与 `log` 冒烟通过。
- 配置字段 round-trip 单测通过（覆盖 M6 新字段）。

## 备注
- 当前 VST2.4 实现定位为“最小宿主 + 回退链打通”。
- 下一步可在 M6 后续迭代中增强插件音频渲染质量与实时参数控制。
