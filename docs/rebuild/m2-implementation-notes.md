# M2 实施记录：WASAPI + 测试 + CI 打包

## M2 目标
- 在 M1 CLI 基线上接入最小 WASAPI 发声能力。
- 保持 `log` 后端作为回归兜底。
- 为 spec-001/spec-003 增加基础自动化测试。
- 增加 Windows CI 与发布包产物脚本。

## 代码变更摘要
- 音频后端：
  - 新增 `include/piano/audio/wasapi_output_sink.h`
  - 新增 `src/audio/wasapi_output_sink.cpp`
  - 扩展 `OutputSink` 生命周期接口：`Start/Stop/Emit`
- 应用层：
  - `src/app/main.cpp` 增加参数：
    - `--audio-backend wasapi|log`
    - `--sample-rate`
    - `--buffer-ms`
  - `src/app/application.cpp` 增加后端选择、失败回退与按 `at_ms` 调度投递。
- 构建：
  - `CMakeLists.txt` 增加 WASAPI 源文件与 `ole32/avrt/uuid` 链接。
- 测试：
  - 新增 `tests/test_main.cpp`
  - 新增回归样例 `assets/regression/ordered.in`
  - 启用 CTest 并注册 `piano_tests`
- 脚本与 CI：
  - 新增 `scripts/package.ps1`
  - 新增 `scripts/ci-local-check.ps1`
  - 更新 `scripts/run-demo.ps1` 支持后端与音频参数
  - 新增 `.github/workflows/windows-ci.yml`

## 本地验证结果
- Debug 构建通过；CTest 通过（`piano_tests`）。
- Release 构建通过；`log` 后端 smoke 通过。
- `wasapi` 后端启动与示例曲谱播放通过（无崩溃）。
- 打包通过：`dist/piano-win-x64.zip`。
- 已提供长时稳定性脚本：`scripts/smoke-wasapi.ps1`（默认 600 秒）。
- 已执行 60 秒 WASAPI 稳定性试跑：通过（15 轮）。

## 当前限制
- WASAPI 目前是“最小可发声”实现，使用短音符 burst 输出验证链路。
- spec-002 的长时稳定性（10 分钟）与设备异常恢复仍需进一步验证。
