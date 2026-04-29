# 代码结构说明（M2）

## 目录概览
- `include/piano`：模块头文件与对外接口。
- `src`：模块实现。
- `tests`：基础自动化测试（CTest）。
- `assets`：示例输入与回归样例。
- `scripts`：本地开发与打包脚本。

## 核心模块
- `input`
  - `KeyboardMap`：加载 `*.keyboard` 并查询键位映射。
- `score`
  - `ScoreParser`：解析 `*.in`（`begin id last db`）。
- `engine`
  - `ScoreScheduler`：将指令转为时序事件（NoteOn/NoteOff/Tempo/Transpose）。
- `audio`
  - `OutputSink`：统一音频输出接口（`Start/Stop/Emit`）。
  - `LogOutputSink`：日志回归后端。
  - `WasapiOutputSink`：WASAPI 最小发声后端。
- `app`
  - `main.cpp`：命令行参数解析与程序入口。
  - `application.cpp`：串联加载、调度、输出、回退逻辑。

## 运行链路
`keyboard/score -> scheduler(events) -> output_sink(log/wasapi)`

## 当前限制
- WASAPI 后端为 M2 最小发声实现，后续将演进为持续混音模型。
