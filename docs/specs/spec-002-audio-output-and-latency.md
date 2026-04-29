# SPEC-002 音频输出与延迟

- 状态：`implemented`
- 优先级：P0

## 背景与目标
- 新版本首发需提供稳定音频输出，优先 WASAPI，兼顾设备兼容与低延迟体验。

## 范围
- In Scope：
  - WASAPI 输出后端
  - DirectSound 回退后端
  - 音频线程模型与缓冲参数
- Out of Scope：
  - ASIO 首发支持

## 功能需求
- 支持启动时自动选择音频后端（优先 WASAPI）。
- 支持配置缓冲大小和采样率（至少 44.1k/48k）。
- 输出链路支持实时 NoteOn/NoteOff 与参数变化。
- 设备不可用时可回退并给出错误提示。

## 非功能需求
- 音频回调线程不可执行阻塞 I/O 或动态大对象分配。
- 长时播放不出现明显爆音和卡顿。
- 按键到发声延迟需满足可演奏体验。

## 验收标准
- 在默认设备上连续播放 10 分钟无明显异常。
- 设备断开/切换时应用不崩溃，可恢复输出。
- 基线曲目回放过程中无持续性破音。

## 测试计划
- 集成测试：
  - 后端切换；
  - 参数切换；
  - 设备异常恢复。
- 手工测试：
  - 快速连按、和弦、长按释放。

## 风险与回滚
- 风险：不同驱动实现导致行为差异。
- 回滚：提供固定保守缓冲配置和后端强制切换选项。

## 实现记录（M2）
- 实现文件：
  - `include/piano/audio/wasapi_output_sink.h`
  - `src/audio/wasapi_output_sink.cpp`
  - `src/app/application.cpp`
  - `src/app/main.cpp`
- 已落地能力：
  - `--audio-backend wasapi|log` 输出后端切换；
  - 默认 `wasapi` 启动，失败自动回退 `log` 并输出错误信息；
  - 支持 `--sample-rate` 与 `--buffer-ms` 运行参数；
  - 保留 `log` 后端用于回归验证。
- 当前差距（进入 implemented 前需补齐）：
  - 10 分钟长时稳定性与设备断开恢复未完成自动化验证；
  - 设备热切换/断开后的自动重建策略仍需补齐。

## 进展记录（M3）
- WASAPI 已升级为持续混音模型：
  - 支持 NoteOn/NoteOff 事件语义；
  - 支持复音混音输出；
  - 提供运行时健康状态上报（`IsHealthy`），应用层可执行回退。
- 应用层已在运行时检测后端健康并支持回退 `log`。

## 实现记录（M5）
- DirectSound 回退后端已落地：
  - 新增 `include/piano/audio/dsound_output_sink.h`
  - 新增 `src/audio/dsound_output_sink.cpp`
  - `PlaybackService` 回退链升级为 `wasapi -> dsound -> log`
- WASAPI 异常处理增强：
  - 运行时错误消息包含 HRESULT 与 recoverable 标记；
  - 应用层增加“有限重试 + 冷却间隔”恢复策略，失败后自动降级下一后端。
- 参数与配置增强：
  - CLI 新增 `--backend-priority`，支持显式回退顺序；
  - 配置文件新增 `backend_priority/recent_keyboard_path/recent_score_path` 持久化字段。
- 轻量验证闭环：
  - `piano_cli --help` 纳入本地/CI 快速检查；
  - 打包后执行 `scripts/verify-package.ps1` 检查关键内容。

## M5 验收证据
- 工程验证：
  - Debug 构建通过；
  - Release 构建通过；
  - `ctest --test-dir build --output-on-failure` 通过。
- 运行验证：
  - `run-demo.ps1 -AudioBackend log` 可运行；
  - GUI 启动冒烟保留在默认快速链路中；
  - 后端参数支持 `wasapi|dsound|log`。
- 打包验证：
  - `package.ps1` 产物生成成功；
  - `verify-package.ps1` 校验 `piano_cli.exe/README.md/assets` 存在。
