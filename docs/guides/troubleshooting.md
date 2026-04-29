# 常见问题排查

## 1) Release 编译失败：`build-releas is not a directory`
原因：命令中目录名拼写错误，`build-release` 少了最后一个 `e`。

正确命令：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Release -BuildDir build-release
```

## 2) `ctest` 找不到
优先使用 VS BuildTools 自带路径：
```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure
```

## 3) `cmake`/`ninja` 不在 PATH
脚本会自动回退到 VS BuildTools 内置 CMake/Ninja；若仍失败，检查 BuildTools 是否完整安装。

## 4) 无法听到声音
- 先用 `-AudioBackend log` 验证事件链路是否正常。
- 再切回 `-AudioBackend wasapi`。
- 检查系统默认输出设备是否可用、音量是否静音。

## 5) M5 回归测试清单（建议每次发布前执行）

### A. 基础链路
1. 构建 Debug：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
```
2. 运行单测：
```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure
```
3. 运行 CLI 帮助：
```powershell
.\build\piano_cli.exe --help
```

### B. 后端切换与回退
1. 日志后端（仅验证事件链）：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend log -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```
2. WASAPI 后端（主路径）：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend wasapi -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```
3. DirectSound 后端（回退路径）：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend dsound -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```
4. 验证点：
- 进程不崩溃；
- 事件日志或发声连续；
- 出错时提示包含错误码前缀（如 `AUDIO_*` / `INVALID_ARGUMENT`）。

### C. GUI 验证
1. GUI 启动冒烟：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-gui-smoke.ps1 -BuildDir build-release -ExitMs 1200
```
2. GUI 手工检查：
- 后端下拉可选 `wasapi/dsound/log`；
- 点 `Play/Stop` 可稳定切换状态；
- 点 `Save Config` 后重启 GUI，配置仍存在。

### D. 打包验证
1. 生成包：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -BuildType Release -BuildDir build-release -OutputDir dist
```
2. 校验包内容：
```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify-package.ps1 -ZipPath dist/piano-win-x64.zip
```
