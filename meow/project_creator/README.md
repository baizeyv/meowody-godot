# Meowody 项目创建器

创建器直接在当前 `meow/` Godot 项目中运行。主场景：`project_creator/main.tscn`；C++ 界面：`src/project_creator/`。

## 启动

在仓库根目录执行 `scons`，用官方 Godot 4.7 Windows x64 打开 `meow/project.godot` 并运行。

界面采用官方 Catppuccin Mocha 配色，初始窗口为宽 1920、高 1080，布局随实际窗口尺寸变化；宽窗口双栏，小窗口上下排列，内容超出时可垂直滚动，弹窗会自动限制在窗口内部。创建器完整使用 C++：TaskCenter 后台执行，SaveCenter 保存偏好，ApplicationRequest 在工作期间拦截退出。HTTPS 使用 WinHTTP，SHA512 使用 Windows BCrypt，ZIP 使用系统 tar.exe。创建器不包含、不提取、不调用 Python 辅助脚本；Python 只属于框架原有的 SCons / 反射编译工具链。

创建工程需要 Git、Python 3.11+、SCons、匹配的 clang Python bindings / LLVM libclang、MSVC C++ 工具和 Windows SDK。程序只检查环境，不安装系统软件。

## 创建流程

1. 输入项目名称，选择父目录，或直接填写一个新目录 / 空目录。文件选择器使用同主题的居中模态弹窗，带背景遮罩、关闭和取消按钮；取消不会改变目标路径。
2. 从下拉框选择框架分支或发布标签，默认 `main`。启动时自动查询，也可点击“刷新”；刷新保留仍存在的选择，查询失败保留现有列表。来源固定为 `https://github.com/baizeyv/meowody-godot.git`。
3. 启动后自动搜索本地引擎，验证 Windows x64 架构、官方稳定版 Godot 4 版本和编辑器能力，过滤控制台副本、预览版与导出模板后列入下拉框。搜索覆盖 PATH、常见安装目录、桌面、下载目录、引擎缓存及各固定磁盘下的工具 / 开发目录（最多向下三层，不跟随链接）。请手动选择一个结果，或使用“浏览引擎文件”补充并校验其他位置。
4. 取消“使用本地 Godot 引擎”后自动查询官方发布接口，在下拉框中默认选中最新稳定版，可手动选择其他稳定版或刷新。查询失败保留已有列表并显示原因；下载仍通过官方 SHA512 校验。
5. 点击“创建项目”：下载框架、生成源码、初始化独立 Git 仓库及固定 godot-cpp 子模块、导出引擎 API、编译 Debug 扩展、验证编辑器导入和框架运行。失败保留输出和日志，不覆盖非空目录。
6. 成功后点击“打开 Godot 项目”。依赖完整时可用创建时的原引擎重新编译并验证。不自动提交、推送或启动编辑器。

新工程 Godot 目录为 `project/`，只复制框架源码、全部框架插件、反射工具、MCP、框架说明与空白场景，不携带创建器、游戏、测试素材或旧二进制。`meowody-project.json` 保存引擎/API/框架版本，`framework-dependencies.json` 和项目元数据共同记录与所选引擎匹配的官方 godot-cpp 标签及提交。优先匹配 Godot 的精确发布标签，其次匹配同一主次版本标签；godot-cpp 10.x 等独立版本仅在框架记录明确声明支持目标 API 时采用，并检查官方标签提交与框架记录一致。旧框架只有 commit、缺少 tag / api_versions 字段时，会核对其固定提交是否就是官方 10.0.0-stable；仅对已验证的 Godot 4.3—4.7 API 恢复兼容映射。显式配置不兼容、未知提交或未知 API 不会被放宽。缺少对应标签或兼容映射则停止，不改用任意分支。编译始终使用所选引擎导出的 extension_api.json。框架仓库与生成工程都使用真实 Git 子模块（父仓库的模式 160000 记录固定提交，.gitmodules 记录官方 URL）。其他机器克隆后执行 `git submodule update --init --recursive` 恢复依赖；提交工程时要同时提交 .gitmodules、子模块记录和依赖锁定文件。

框架远程 ref 必须包含依赖锁定文件和支持项目配置的 SConstruct；尚未推送的本地修改不会出现在从远程生成的工程中。

偏好、引擎缓存、任务请求和运行日志位于 Godot `user://`；工程日志位于新工程 `build/creator/`。界面显示最近 64 KiB 日志，按面板宽度自动换行，只允许垂直滚动。任务执行期间禁止关闭窗口，完成回调通过 ObjectID 验证界面仍存活。

## 导出与验证

Windows Desktop 预设已配置为 Windows x64，默认导出到 `build/project-creator/windows-x64/MeowodyProjectCreator.exe`。资源包嵌入 EXE，原生扩展 DLL 保留在同一目录；分发时复制整个目录。导出不生成控制台窗口，产品名和版本信息已填写。创建器自身仅包含 Godot 资源与原生扩展。导出前执行 `scons target=template_release`，安装匹配的官方 Godot 导出模板，并通过资源面板构建 Windows 资源目录（空条目也需要构建）。也可执行 `python meow/addons/meowody_resources/build.py --project meow --godot "Godot.exe的完整路径" --platform windows`。本机的官方 Godot 4.7.2 Windows Debug / Release 模板已通过官方 SHA512 校验并安装。重新打开项目后，选择“项目 → 导出 → Windows Desktop → 导出项目”，发布版取消“导出调试”即可。其他机器仍需安装对应版本的模板。导出后的创建器仍需上述构建工具；请从检测结果中手动选择用于生成项目的 Godot 编辑器。

```powershell
scons -f tests/SConstruct.project_creator
./build/project-creator/native-tests/creator_backend_test.exe --online
python tests/run_project_creator_ui_test.py --godot "D:/path/to/Godot.exe" --visual
python tests/run_project_creator_export_test.py --godot "D:/path/to/Godot.exe"
python tests/run_project_creator_test.py --godot "D:/path/to/Godot.exe" --dependency "固定提交的 godot-cpp 检出目录"
```

Godot 测试串行复用 `tests/workspace/meow`，隔离用户配置。UI 回归验证真实引擎搜索、官方版本下拉框、悬停前后文字像素一致、后台失败恢复、目录保护、偏好与退出拦截；原生后端回归检查架构与版本筛选、参数引用、SHA512、依赖标签匹配、源码过滤及目录保护；生成回归调用原生模板生成器并真实编译、导入和运行新工程。Python 测试驱动只用于开发验证，不随创建器运行。

发布构建保留 Godot C++ 绑定所需的 RTTI；即使关闭框架异常，也不能对原生节点禁用 RTTI。导出回归会分别启动 Debug 和 Release EXE，防止仅编译成功而发布程序不能启动。
