# Meowody 项目创建器

创建器直接在当前 `meow/` Godot 项目中运行。主场景：`project_creator/main.tscn`；C++ 界面：`src/project_creator/`。

## 启动

在仓库根目录执行 `scons`，用官方 Godot 4.7 Windows x64 打开 `meow/project.godot` 并运行。

界面使用框架 TaskCenter 后台执行、SaveCenter 保存偏好和 ApplicationRequest 退出拦截。Python 仅负责构建辅助任务，随 Godot 资源发布，运行时提取到用户数据目录。已移除独立 Tkinter 窗口和 PyInstaller 打包链。

创建工程需要 Git、Python 3.11+、SCons、匹配的 clang Python bindings / LLVM libclang、MSVC C++ 工具和 Windows SDK。程序只检查环境，不安装系统软件。

## 创建流程

1. 输入项目名称，选择父目录，或直接填写一个新目录 / 空目录。
2. 选择框架分支、标签或提交，默认 `main`。来源固定为 `https://github.com/baizeyv/meowody-godot.git`。
3. 选择本地官方 Godot EXE，或取消“使用本地引擎”并填写 `4.x.y-stable`。可获取官方稳定版本列表；下载通过官方 SHA512 校验。
4. 点击“创建并验证”：下载框架、生成源码、初始化独立 Git 仓库及固定 godot-cpp 子模块、导出引擎 API、编译 Debug 扩展、验证编辑器导入和框架运行。失败保留输出和日志，不覆盖非空目录。
5. 成功后点击“打开 Godot 项目”。依赖完整时可用创建时的原引擎重新编译并验证。不自动提交、推送或启动编辑器。

新工程 Godot 目录为 `project/`，只复制框架源码、全部框架插件、反射工具、MCP、框架说明与空白场景，不携带创建器、游戏、测试素材或旧二进制。`meowody-project.json` 保存引擎/API/框架版本，`framework-dependencies.json` 固定官方 godot-cpp 提交。其他机器克隆后执行 `git submodule update --init --recursive` 恢复依赖。

框架远程 ref 必须包含依赖锁定文件和支持项目配置的 SConstruct；尚未推送的本地修改不会出现在从远程生成的工程中。

偏好、引擎缓存、任务请求和运行日志位于 Godot `user://`；工程日志位于新工程 `build/creator/`。界面显示最近 64 KiB 日志。任务执行期间禁止关闭窗口，完成回调通过 ObjectID 验证界面仍存活。

## 导出与验证

Windows Desktop 预设导出到 `build/project-creator/`，包含 Python 辅助脚本。导出前执行 `scons target=template_release`，安装匹配的官方 Godot 导出模板，并通过资源面板构建 Windows 资源目录（空条目也需要构建）。也可执行 `python meow/addons/meowody_resources/build.py --project meow --godot "Godot.exe的完整路径" --platform windows`。导出后的创建器仍需上述构建工具；请手动选择用于生成项目的 Godot 编辑器 EXE。

```powershell
python -m unittest discover -s tests -p test_project_creator.py
python tests/run_project_creator_ui_test.py --godot "D:/path/to/Godot.exe" --visual
python tests/run_project_creator_test.py --godot "D:/path/to/Godot.exe" --dependency "固定提交的 godot-cpp 检出目录"
```

Godot 测试串行复用 `tests/workspace/meow`，隔离用户配置。UI 回归验证界面加载、后台失败恢复、目录保护、偏好保存与退出拦截；生成回归验证真实编译、导入与运行。
