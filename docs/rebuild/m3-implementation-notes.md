# M3 实施记录（当前迭代）

## 目标
- 将 WASAPI 从固定 burst 发声升级为持续混音模型。
- 增加运行时异常感知与回退链路。
- 不增加默认 CI 时长，保持快速反馈。

## 已完成
- 音频输出：
  - `src/audio/wasapi_output_sink.cpp` 改为渲染线程持续填充缓冲。
  - 支持 NoteOn/NoteOff 与复音混音。
  - 增加后端健康上报接口：`OutputSink::IsHealthy`。
- 应用层：
  - `src/app/application.cpp` 在播放过程中检测后端健康；
  - WASAPI 异常时自动回退 `log` 后端继续流程。
- 测试：
  - `tests/test_main.cpp` 新增调度顺序与非法 token 回归检查。
- CI 策略：
  - `.github/workflows/windows-ci.yml` 保持默认快速链路；
  - 长时 WASAPI smoke 改为 `workflow_dispatch` 手动触发。
- CLI：
  - `src/app/main.cpp` 增加 `--help`；
  - 增加参数合法性校验（后端与数值参数）。

## 本地验证
- Debug/Release 构建通过。
- CTest 通过。
- `--help` 输出与错误参数提示通过。
- `log` 与 `wasapi` 路径均可启动。

## 后续缺口
- 设备断开/热切换后的自动恢复策略需进一步强化。
- 10 分钟稳定性可手动跑，但默认 CI 不执行。
