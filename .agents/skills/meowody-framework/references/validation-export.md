# 验证、调试、性能、MCP 与导出

适用：修复验证、泄漏、原生崩溃、性能问题、操作当前编辑器和发布。手册：#debug、#performance、#mcp、#verification、#troubleshooting。

## 选择验证入口

先检查测试文件存在。框架仓库通常提供下列入口；创建器默认不分发 tests，因此生成项目使用自身测试目标，不能运行不存在的框架命令。

| 改动 | 框架入口（存在时使用） |
| --- | --- |
| 反射、绑定和热重载 | `tests/` 中 reflect/binding 驱动，按实际文件选择 |
| 主线程、任务和输入 | `tests/run_centers_test.py`、`tests/async_centers_test.cpp` |
| 应用生命周期与退出 | `tests/run_lifecycle_test.py --visual` |
| 地址资源和资源包 | `tests/run_resources_test.py` |
| Atlas / MultiPatch | `tests/run_atlas_editor_test.py`、`tests/run_multipatch_editor_test.py` |
| MCP | `tests/run_mcp_editor_test.py` |
| 创建器 | `tests/run_project_creator_test.py`、`tests/run_project_creator_ui_test.py`、`tests/run_project_creator_export_test.py` |

读取驱动 --help/源码后传实际 --godot，不假定所有驱动参数相同。框架 Godot 测试统一通过 `tests/godot_test_workspace.py::prepare_workspace()` 使用 `tests/workspace/meow`；串行运行，重置前结束上次测试编辑器，保留要比较的日志。用户真实编辑器不属于测试进程，保护未保存场景。Windows 测试隔离 APPDATA、LOCALAPPDATA、TEMP、TMP，GPU 像素验证使用 visual。新项目建立自身 tests 入口时沿用隔离原则，不要求复制整个框架测试集。

## 泄漏与崩溃

先保存实际引擎版本、命令、目标配置和完整日志，用 `--verbose` 查 `Leaked instance` 的类型及 ID。区分编辑器导入退出和游戏运行退出，定位创建者及释放责任；普通 Object、脱树 Node、循环引用、未断开信号分别排查。不要通过隐藏警告或强退进程“修复”泄漏。

原生崩溃先确认 DLL、PDB/符号与源码匹配，查看 `src/log/`、`src/runtime/debug/` 和 `addons/meowody_debug/` 的现有能力，再缩小复现。Release 注册阶段崩溃也检查 RTTI 是否被错误关闭。仅在任务涉及崩溃处理时运行故意 CrashHandler 测试，且只针对隔离进程。

## 性能

先留存改动前二进制/版本及相同工作负载，再测量。框架 Core/Save/Service/Arc 基准的普通 C++ 分配计数不包含引擎分配，不能直接宣称 FPS 改善。保留行为正确性、自取消、回调变更、GPU 像素等验证。只优化证实的热点，不因性能任务关闭反射诊断或牺牲存档快照隔离。

## MCP 与用户当前编辑器

存在 `tools/godot_mcp/` 时可在其目录运行 `npm ci --ignore-scripts`、`npm run build`，再用 `node tools/godot_mcp/dist/index.js --project <Godot根绝对路径>` 启动。先查看实际连接可用性，配置文件存在不代表聊天已连接；客户端可能需要重启加载配置。

先调用 editor_get_status 与 editor_get_scene_tree 核对项目和当前场景。保护未保存修改、场景所有权和原生 UndoRedo；拒绝越过项目/场景边界的节点与路径。属性使用 Godot literal，资源/节点按工具协议用 load(...)、node(...)。绑定工作进行中不并发修改。新 native 类型仍需编译注册，MCP 不能替代它。

插件是 GDScript EditorPlugin，无运行时 autoload。使用 loopback WebSocket、每实例 token 和项目身份，端点在 Godot 根 `.godot/meowody_mcp`，正常退出删除。不要输出 token、分发本机 `.codex/config.toml` 或破坏其他 MCP 服务器配置。工具不可用时使用源码与隔离验证，不虚构工具结果。

## 导出

1. 确认实际引擎版本、平台/架构、对应官方导出模板和 Debug/Release native 库；模板需要下载时核对官方版本和校验和。
2. 根据资源设置先构建地址目录和 PCK 依赖，再检查导出预设包含启动场景、gdextension、运行时素材与所需 DLL。测试/编辑器内容的排除按 [编辑器与构建边界](editor-tools.md) 实施。
3. 读取实际 `export_presets.cfg` 确定输出位置、PCK 内嵌或独立，不把创建器的预设强加给生成游戏。生成项目没有预设时为用户目标单独配置，不复制包含创建器场景的预设。
4. 在干净输出目录导出，并实际启动导出程序。分别报告 Debug/Release 是否验证；启动场景成功不替代真实业务资源加载。检查缺失 DLL、PCK、资源目录、地址初始化和退出泄漏。

原生 DLL 在 Windows 仍是外部文件，内嵌 PCK 不代表只有 exe。不要自动提交、推送或发布；交付说明真实输出及已验证范围，移动端模拟不能声称真机已测。
