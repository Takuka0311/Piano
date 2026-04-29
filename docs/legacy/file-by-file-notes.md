# 旧版关键文件逐项说明

## 工程与构建文件
- `reference/Piano/Piano.sln`
  - 单项目解决方案，x64 Debug/Release。
- `reference/Piano/Piano/Piano.vcxproj`
  - Qt VS Tools 工程（Qt 5.14.2）。
  - 关键源文件：`Piano.cpp`、`msgQueue.cpp`、`vst.cpp`。
  - 资源输入：`init.keyboard`、`*.in`、`*.keyboard`、`mdaPiano.dll`。

## 业务代码
- `reference/Piano/Piano/Piano.cpp`
  - 单体主业务文件，包含初始化、输入处理、播放调度、音频发送。
  - 可复用资产：
    - `toPianoKey` token 解释规则。
    - `getString` 音名文本生成。
  - 主要问题：
    - 逻辑集中，缺少边界抽象。
    - 多处状态变量共享修改（`lay/db/speed/isPlay`）。
- `reference/Piano/Piano/msgQueue.cpp`
  - 读取 `*.in` 并构建“开始/结束事件”优先队列。
  - 可复用资产：
    - 双队列事件调度思想，可迁移到新引擎。
  - 主要问题：
    - 缺少输入校验与异常处理。
- `reference/Piano/Piano/vst.cpp`
  - 最小 VST2 host 封装：动态加载 DLL、投递 MIDI、processReplacing。
  - 可复用资产：
    - VST 事件打包与处理调用顺序。
  - 主要问题：
    - 全局静态状态多，线程安全不可控。
    - 采样率/缓冲写死为 44100/32。

## UI 与资源
- `reference/Piano/Piano/Piano.ui`
  - 使用完整键盘布局按钮做映射和可视化。
- `reference/Piano/Piano/init.keyboard`
  - 默认按键映射表。
- `reference/Piano/Piano/カワキヲアメク.keyboard`
  - 曲目专用映射，可播放时覆盖默认映射。
- `reference/Piano/Piano/梦中的婚礼.in`
- `reference/Piano/Piano/カワキヲアメク.in`
  - 曲谱脚本数据（包含大量力度、速度、复音信息）。
- `reference/Piano/mdaPiano.dll`
  - 核心乐器插件依赖，属于运行时关键资产。

## 建议重构时保留/迁移优先级
- P0（必须保留）
  - `*.keyboard` 与 `*.in` 的兼容读取能力。
  - token 语义（调式、速度、音高规则）。
  - VSTi 与 MIDI 输出两条能力路径（至少保留其一并为另一条预留接口）。
- P1（建议保留）
  - UI 键盘可视化按压反馈。
  - 多曲目/多布局切换。
- P2（可重设计）
  - 当前按钮命名和快捷键绑定方式。
  - 当前主线程播放调度实现。
