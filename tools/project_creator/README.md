# Meowody 项目创建器

Windows x64 桌面程序，从固定仓库 `https://github.com/baizeyv/meowody-godot.git` 的指定分支、标签或提交创建独立的框架项目。默认分支为 `main`，新工程的 Godot 目录固定为 `project/`。

## 启动与打包

源码启动（Python 3.11+，包含 Tkinter）：

```powershell
python tools/project_creator/app.py
```

生成无需 Python 即可启动的 EXE：

```powershell
./tools/project_creator/build.ps1
```

打包脚本在 `build/project-creator/packaging` 创建独立 Python 环境，不向系统 Python 安装 PyInstaller。输出为 `build/project-creator/dist/MeowodyProjectCreator.exe`。

创建器 EXE 自身不需要 Python，但编译框架需要本机 Git、Python、SCons、MSVC C++ 工具和 Windows SDK，以及匹配的 clang Python bindings / LLVM libclang。编辑器热重载也需要 Python 和 SCons 在 PATH 中。没有安装工具时，程序显示缺失项，不自动安装系统软件。

## 创建流程

1. 填写项目名称并选择父目录，程序生成对应的目标路径；也可手动指定一个新目录或空目录。
2. 选择框架 ref。选择下载官方 Godot 稳定版本，或已有的 Godot EXE。支持标准 Windows x64 单精度 Godot 4；4.7 为验证基线，其他版本会实际编译验证，不承诺旧 API 兼容。
3. 点击“创建并验证”。程序获取框架，准备固定提交的 godot-cpp，复制框架源码、全部框架插件及工具，生成空白主场景。
4. 从选定引擎导出 API，编译 Debug 扩展，验证编辑器导入、原生类注册和空白场景运行。
5. 成功后点击“打开 Godot 项目”。程序不会自动提交、推送或启动编辑器。

引擎版本来自官方 godot-builds Git 标签，发布包通过官方 SHA512 校验；引擎和上次选择保存在 `%LOCALAPPDATA%/MeowodyProjectCreator`。私有 Git 来源复用本机凭据，不保存密码或令牌。失败会保留已生成的文件；编译失败可使用“重新编译并验证”，依赖下载尚未完成则选择新的空目录重试。日志在新工程的 `build/creator/`。

**首次使用前，请将本次的 `framework-dependencies.json`、SConstruct 和创建器改动推送到框架远程仓库。** 缺少固定依赖记录的旧 ref 会被拒绝。godot-cpp 有子模块记录时初始化子模块并检查提交一致性；没有记录时从官方仓库检出锁定提交，不依赖 `.gitmodules` 的分支。

## 新工程

```text
新工程/
├── project/               project.godot、main.tscn、addons、bin
├── src/                   runtime、editor、log、reflection、注册入口
├── godot-cpp/             固定提交的官方源码
├── godot-api/             所选引擎导出的扩展 API
├── reflect_codegen/
├── tools/godot_mcp/
├── .agents/skills/meowody-framework/
├── framework-dependencies.json
├── meowody-project.json
├── FRAMEWORK.md
└── SConstruct
```

不复制现有游戏、TestNode、业务生成类、原仓库 Git 元数据、测试工程、旧二进制或缓存。新项目持有源码，可以独立开发。`scons` 和编辑器热重载共同读取 `meowody-project.json` 中的 API 配置；显式传入 `api_version` 或 `custom_api_file` 时以命令行参数为准。当前框架仓库仍使用 `meow/`，未配置文件时构建行为不变。

MCP 工具源码包含在新工程中；使用其外部服务时另外安装 Node.js 20+ 并运行 `npm ci --ignore-scripts`、`npm run build`，启动时 `--project` 指向新工程的 `project/` 绝对路径。

## 验证

```powershell
python -m unittest discover -s tests -p test_project_creator.py
python tests/run_project_creator_test.py --godot "D:/path/to/Godot.exe" --dependency "固定提交的 godot-cpp 检出目录"
```

真实构建测试只复用 `tests/workspace/meow` 这个测试工作区；它此时是生成工程的根目录，唯一 Godot 项目位于其 `project/`。测试串行运行，会重置上次测试内容。完整编译和验证日志保存到 `build/test-results/project-creator.log`。
