# 开发、构建与发布流程（Windows）

## 当前环境评估
- 已检测到：Visual Studio 2022 Build Tools（含 VC 工具链）。
- 需补齐并加入 PATH：
  - `cmake`
  - `ninja`
- 建议保留统一入口脚本，避免依赖 IDE 手工操作。

## 本地开发方式（Cursor + 命令）
- 编辑器：Cursor。
- 构建：仅使用脚本命令，不依赖 VS IDE 工程按钮。
- 推荐目录约定：
  - `scripts/configure.ps1`
  - `scripts/build.ps1`
  - `scripts/package.ps1`
  - `scripts/ci-local-check.ps1`

## 脚本职责
- `configure.ps1`
  - 进入 VS 开发者环境（或显式指定编译器）。
  - 生成 `build/`（Ninja + CMake）。
- `build.ps1`
  - 编译目标（Debug/Release）。
- `package.ps1`
  - 收集 exe、配置、必要运行时，生成发布包。
- `ci-local-check.ps1`
  - 运行格式检查、基础测试、打包预演。

## GitHub Actions 发布方案
- 触发：
  - `push` 到发布分支或 `tag`。
- 阶段：
  1. 安装 CMake + Ninja。
  2. 配置构建（Release）。
  3. 编译并执行基础测试。
  4. 打包可运行产物（zip）。
  5. 创建 GitHub Release 并上传附件。

## 产物要求
- 目标：用户下载后可直接运行 `.exe`。
- 发布包内至少包含：
  - 主程序 exe
  - 默认键位/示例曲谱
  - 必要依赖文件（若有）
  - 简要使用说明

## 最小 CI 示例（策略）
- 使用 `windows-latest` runner。
- 默认生成 x64 Release。
- 保持单命令构建链：`configure -> build -> package`。

## 质量门禁建议
- PR 门禁：
  - 构建通过；
  - 关键解析单测通过；
  - 回放基线用例通过。
- Release 门禁：
  - 产物启动自检通过；
  - 关键键位映射验证通过；
  - 基线曲谱可完整播放。
