# SPEC-004 MIDI In/Out 与 VSTi

- 状态：`review`
- 优先级：P1（其中 MIDI Out + VSTi 接入为 P0 子集）

## 背景与目标
- 保持旧版可用的乐器与 MIDI 输出路径，逐步扩展 MIDI 输入能力。

## 范围
- In Scope：
  - MIDI Out 设备枚举与输出
  - VSTi 插件加载、MIDI 投递、音频渲染
  - MIDI In（预留接口，逐步实现）
- Out of Scope：
  - VST3 与复杂插件管理功能

## 功能需求
- 启动时可选择“VSTi 输出”或“系统 MIDI 输出”。
- VSTi 加载失败时可回退到 MIDI Out 路径。
- 支持发送标准 NoteOn/NoteOff 与基础控制消息。
- 支持设备切换与基本错误提示。

## 非功能需求
- 插件处理链路不阻塞 UI。
- 设备异常不会导致进程崩溃。

## 验收标准
- 可加载基线 VSTi 并发声。
- 切换输出路径后可继续演奏。
- MIDI Out 在目标设备上可收到正确消息。

## 测试计划
- 集成测试：
  - 插件加载/卸载；
  - 输出切换；
  - 异常恢复。
- 兼容测试：
  - 使用 `mdaPiano.dll` 验证最小兼容闭环。

## 风险与回滚
- 风险：VST 插件行为差异大，宿主稳定性挑战高。
- 回滚：限制首版插件能力范围，保留 MIDI Out 兜底模式。

## 实现记录（M6）
- 新增输出后端：
  - `include/piano/audio/midi_output_sink.h`
  - `src/audio/midi_output_sink.cpp`
  - `include/piano/audio/vsti_output_sink.h`
  - `src/audio/vsti_output_sink.cpp`
- 回退链升级：
  - `vsti -> midiout -> wasapi -> dsound -> log`
  - `output_mode=vsti` 时优先 VSTi，失败自动降级 MIDI Out。
- CLI + GUI 双入口：
  - CLI 新增 `--output-mode/--midi-out-device/--vsti-plugin`
  - GUI 新增输出模式选择、MIDI 设备输入、VSTi DLL 路径输入/浏览
- 配置持久化：
  - 新增 `output_mode/midi_out_device/vsti_plugin_path` 字段
  - 保持 `backend_priority` 与最近文件字段联动。

## M6 验证记录
- 已完成：
  - Debug/Release 构建通过；
  - `ctest` 通过；
  - CLI `--help` 与 `log` 路径冒烟通过；
  - 配置字段 round-trip 单测通过（含 M6 新字段）。
- 手动触发验证入口：
  - `scripts/smoke-midiout.ps1`
  - `scripts/smoke-vsti.ps1 -VstiPluginPath <mdaPiano.dll>`
  - CI `workflow_dispatch` 支持 `run_midiout_smoke/run_vsti_smoke`。

## 当前差距（进入 implemented 前）
- VSTi 真正“插件音频渲染回 WASAPI”仍为后续增强项；当前最小宿主聚焦插件加载与 MIDI 事件链路。
- 需补齐目标环境下 `mdaPiano.dll` 的人工验收记录，再推进到 `implemented`。
