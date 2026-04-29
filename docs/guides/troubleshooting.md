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
