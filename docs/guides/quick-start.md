# 快速开始（Windows）

## 前置条件
- Windows 10/11
- Visual Studio 2022 Build Tools（含 C++ 工具链）

## Debug 构建与运行
在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend wasapi -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```

如需日志模式（不发声，仅看事件）：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend log -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```

如需 DirectSound 回退链验证：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build -AudioBackend dsound -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
```

## Release 构建与运行
```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Release -BuildDir build-release
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Release -BuildDir build-release
powershell -ExecutionPolicy Bypass -File scripts/run-demo.ps1 -BuildDir build-release -AudioBackend wasapi -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in -ProbeKey Q
.\build-release\piano_gui.exe
```

## 运行测试
```powershell
powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildType Debug -BuildDir build
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildType Debug -BuildDir build
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure
```

## 查看命令行帮助
```powershell
.\build\piano_cli.exe --help
```

## 打包
```powershell
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -BuildType Release -BuildDir build-release -OutputDir dist
powershell -ExecutionPolicy Bypass -File scripts/verify-package.ps1 -ZipPath dist/piano-win-x64.zip
```

## 一键本地 CI 检查
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ci-local-check.ps1 -DebugDir build -ReleaseDir build-release
```

## GUI 启动冒烟（本地/CI）
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-gui-smoke.ps1 -BuildDir build-release -ExitMs 1200
```

## WASAPI 长时稳定性（10 分钟）
```powershell
powershell -ExecutionPolicy Bypass -File scripts/smoke-wasapi.ps1 -BuildDir build-release -DurationSeconds 600 -KeyboardPath assets/default.keyboard -ScorePath assets/demo.in
```

说明：默认 GitHub CI 不运行该长时测试；建议本地或手动触发 workflow 时执行。
