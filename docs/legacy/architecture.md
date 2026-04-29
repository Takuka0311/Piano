# 旧版架构与数据流

## 模块划分
- UI/交互层：`Piano.ui` + `Piano.cpp`
  - 负责按键控件、快捷键、Play/Stop 按钮、UI 状态切换。
- 事件调度层：`msgQueue.cpp`
  - 将 `*.in` 音符事件拆成“开始/结束”两类时序事件，按时间弹出。
- 音频输出层：`Piano::midiOut` + `vst.cpp`
  - 路径 A：VSTi（`mdaPiano.dll`）进程内合成。
  - 路径 B：WinMM MIDI 设备输出。
- 配置/资产层：`*.keyboard`、`*.in`、`mdaPiano.dll`

## 主流程
1. 程序启动创建 `QApplication`，显示 `Piano` 主窗体。
2. 构造函数中依次执行：
   - `initVariable()`
   - `initHandle()`（打开默认 MIDI out）
   - `initkeyBoard()`（收集按钮和快捷键）
   - `initPianoKeys("init.keyboard")`
   - `vsti_load_plugin()`
3. 用户按键触发 `keyPressEvent/keyReleaseEvent`，转发到对应按钮事件。
4. 按钮按下/释放回调调用 `midiOut(pianoKey)`。
5. 若点击 Play，进入 `play(songName)` 从 `msgQueue` 逐事件驱动。

## 关键数据结构
- `std::map<QString, QPushButton*> keyBoard`
  - 键名到 UI 按钮的映射。
- `std::map<int, QString> pianoKeys`
  - MIDI key 到键名的反向映射。
- `bool flag[128]`
  - 每个 MIDI key 是否处于按下态。
- `msgQueue`
  - `q1`：按开始时间排序。
  - `q2`：按结束时间排序。

## 输出数据流
- 输入事件（按键或曲谱） -> 生成 `pianoKey` -> NoteOn/NoteOff。
- VST 路径：
  - `vsti_send_midi_event(...)`
  - `vsti_update_config(44100, 32)`
  - `vsti_process(left, right, 32)`
- WinMM 路径：
  - `midiOutShortMsg(handle, ...)`

## 控制 token 规则（`toPianoKey`）
- 调式/移调 token：`A/B/C/D/E/F/G` 与 `a/b/c/d/e/f/g`
- 速度 token：`S`（将 `speed` 设为 `db`）
- 休止 token：`0`
- 音高 token：`1..7` + 可选 `#` + 八度偏移 `+/-`

## 设计问题摘要
- UI、调度、音频耦合在 `Piano.cpp` 单类中。
- 播放时序由主线程忙等待实现，抖动风险高。
- VST 与系统 MIDI 路径判定逻辑可读性差（函数名与分支语义不一致）。
- 资源与曲目选择硬编码，缺少配置层。
