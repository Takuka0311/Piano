# M1 实施记录：最小可运行闭环

## 实施目标
- 建立 Windows 下最小工程骨架（CMake + Ninja + MSVC toolchain）。
- 落地 spec-001 与 spec-003 的最小实现。
- 打通 `输入 -> 事件 -> 输出` 主链路。

## 新增模块与职责
- `input`
  - 文件：`include/piano/input/keyboard_map.h`、`src/input/keyboard_map.cpp`
  - 能力：加载 `*.keyboard`，提供键名到 MIDI key 查询。
- `score`
  - 文件：`include/piano/score/score_parser.h`、`src/score/score_parser.cpp`
  - 能力：加载 `*.in`，解析 `begin id last db` 并按时间排序。
- `engine`
  - 文件：`include/piano/engine/score_scheduler.h`、`src/engine/score_scheduler.cpp`
  - 能力：解释 token 并生成时序事件（NoteOn/NoteOff/TempoChange/TransposeChange）。
- `audio`
  - 文件：`include/piano/audio/output_sink.h`、`include/piano/audio/log_output_sink.h`、`src/audio/log_output_sink.cpp`
  - 能力：日志输出占位后端，作为后续 WASAPI/MIDIOut 适配基线。
- `app`
  - 文件：`include/piano/app/application.h`、`src/app/application.cpp`、`src/app/main.cpp`
  - 能力：串联 keyboard + score + scheduler + output，形成可执行程序。

## 已实现的 spec-001 / spec-003 子集
- spec-001（键盘映射）
  - 支持 `<KeyName> <MidiKey>` 解析。
  - 支持查询映射（示例：`--probe-key Q`）。
  - 支持注释行与空行忽略。
- spec-003（曲谱与调度）
  - 支持 `begin id last db` 解析。
  - 支持 token：`1..7`、`#`、`+/-`、`0`、`S`、`A..G/a..g`。
  - 生成开始/结束事件并按时间稳定排序。

## 示例数据
- `assets/default.keyboard`
- `assets/demo.in`

## 本地命令
```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```

## 当前限制
- M1 输出仍为日志占位，尚未接入真实音频设备。
- 尚未引入自动化单测框架（M2 补齐）。

## 自检结果
- Debug:
  - `scripts/configure.ps1 -BuildType Debug -BuildDir build` 通过。
  - `scripts/build.ps1 -BuildType Debug -BuildDir build` 通过。
  - `scripts/run-demo.ps1 -BuildDir build ...` 通过，输出 15 条有序事件。
- Release:
  - `scripts/configure.ps1 -BuildType Release -BuildDir build-release` 通过。
  - `scripts/build.ps1 -BuildType Release -BuildDir build-release` 通过。
  - `scripts/run-demo.ps1 -BuildDir build-release ...` 通过，输出与 Debug 一致。
