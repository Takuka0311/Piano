# M5 实施记录（稳定性与兼容）

## 范围确认
- 仅执行 M5 稳定性与兼容任务。
- 不包含 MIDI Out / VSTi 功能开发（保留到 M6）。

## 代码落地

### 1) DirectSound 回退后端
- 新增 `DsoundOutputSink`：
  - `include/piano/audio/dsound_output_sink.h`
  - `src/audio/dsound_output_sink.cpp`
- 在 `PlaybackService` 中形成统一回退链：
  - 首选后端可配置；
  - 失败后按顺序回退，默认 `wasapi -> dsound -> log`。
- `CMakeLists.txt` 接入新源文件与链接库（`dsound/dxguid/winmm`）。

### 2) WASAPI 异常恢复策略
- `WasapiOutputSink` 记录运行时 HRESULT，并标注 `recoverable`。
- `PlaybackService` 引入恢复策略：
  - 有限重试（2 次）；
  - 每次重试前冷却 150ms；
  - 重建失败后继续按后端链回退，不直接崩溃。

### 3) 配置持久化与错误体系统一
- 新增统一错误码定义：`include/piano/app/error_codes.h`。
- `UiConfig` 扩展并持久化：
  - `backend_priority`
  - `recent_keyboard_path`
  - `recent_score_path`
- CLI 与 GUI 运行后均写回最近使用配置。
- CLI 新增参数：`--backend-priority`。

### 4) CI 与轻量验证
- 默认快速 CI 新增：
  - `piano_cli --help` 自检；
  - `scripts/verify-package.ps1` 包内容校验。
- 本地 `scripts/ci-local-check.ps1` 同步加入上述检查。

## 自检结果
- Debug 配置与构建通过。
- Release 配置与构建通过。
- 测试通过：`ctest --test-dir build --output-on-failure`。
- 冒烟通过：
  - `piano_cli --help`
  - `run-demo.ps1 -AudioBackend log`
- 打包校验通过：`verify-package.ps1`。

## 与 spec 对齐
- `spec-002` 已补齐 M5 证据并更新为 `implemented`。
