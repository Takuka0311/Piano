# 旧版 Piano 总览

## 项目定位
- 这是一个 Windows 桌面钢琴程序，核心交互是“键盘按键 -> MIDI 音符输出”。
- UI 基于 Qt Widgets；声音输出同时支持：
  - 通过 `mdaPiano.dll` 作为 VSTi 在进程内合成；
  - 通过 WinMM `midiOutShortMsg` 输出到系统 MIDI 设备。
- 内置曲谱播放能力：读取 `*.in` 与 `*.keyboard` 文本文件，按时间调度自动按键与发音。

## 代码与工程入口
- 解决方案：`reference/Piano/Piano.sln`
- 主工程：`reference/Piano/Piano/Piano.vcxproj`
- 应用入口：`reference/Piano/Piano/main.cpp`
- 主窗口与业务：`reference/Piano/Piano/Piano.h`、`reference/Piano/Piano/Piano.cpp`
- 时间队列：`reference/Piano/Piano/msgQueue.h`、`reference/Piano/Piano/msgQueue.cpp`
- VST host 封装：`reference/Piano/Piano/vst.h`、`reference/Piano/Piano/vst.cpp`

## 主要功能清单
- 实时键盘演奏：
  - 将键盘按键（如 `Q`、`W`、`A`、`Esc` 等）映射到 MIDI key。
  - 按下/释放时分别发送 NoteOn/NoteOff。
- 可切换键位布局：
  - 默认布局来自 `init.keyboard`。
  - 演奏曲目时可切换到曲目专属 `*.keyboard`。
- 自动播放：
  - 点击 `Space`（Play）开始播放当前曲目（当前硬编码为 `カワキヲアメク`）。
  - 点击 `Menu`（Stop）停止并恢复默认键位映射。
- 曲谱脚本能力：
  - `*.in` 支持音符、时值、力度，以及“速度/调式变化”控制事件。
- 可视化反馈：
  - UI 按钮按下态与释放态同步显示当前键位触发。

## 数据文件格式（从现有实现反推）
- `*.keyboard`：`按键名 MIDIKey`
  - 例如 `Q 60`、`A 48`。
- `*.in`：`begin id last db`
  - `begin`：起始拍点（浮点）
  - `id`：音符/控制 token（如 `6+`、`C`、`S`）
  - `last`：持续拍数
  - `db`：力度或控制参数

## 已知行为特征
- 默认参数：
  - `lay = 0`（移调）
  - `db = 80`（力度）
  - `speed = 1000`（ms/拍）
- 自动播放时间驱动依赖忙等待 + `QCoreApplication::processEvents()`，非高精度时钟线程。
- MIDI 输出分支由 `vsti_is_instrument_loaded()` 判定选择路径。

## 当前版本的价值
- 代码体量小，业务链路直接，适合作为重构需求基线。
- 已具备“键位映射 + 曲谱回放 + 力度/调式/速度控制 + VSTi 接入”最小闭环。
