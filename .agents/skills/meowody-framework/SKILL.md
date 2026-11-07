---
name: meowody-framework
description: 开发、修复、审查 Meowody Godot C++ 框架及其生成的游戏项目，包括业务系统、原生 UI、资源、编辑器工具、测试与导出。用于包含 SConstruct 和 src/runtime/app.h 的 Meowody 工作区。
---

# Meowody 框架与游戏开发

适用于框架仓库，也适用于创建器生成的新项目。先识别当前布局，再按任务读取下面的参考文档；无需一次加载所有模块。

## 先确认环境

1. 以 `SConstruct`、`src/runtime/app.h` 所在目录作为项目根。存在 `meowody-project.json` 时读取 `project_dir`、`api_version`、`custom_api_file` 及依赖版本；未配置时 Godot 根默认是 `meow/`。不要把根目录与 `res://` 混为一谈。
2. 手册优先使用根目录 `FRAMEWORK.md`，否则使用 `README.md`；下文“手册 #anchor”指该文件的对应锚点。源码路径以项目根为基准，`addons/` 路径以 Godot 根为基准。
3. 查看实际源码、构建选项和当前改动后再修改。框架专用的创建器、测试驱动可能未随新项目分发；调用前确认文件存在。不要从本机路径推断工具、引擎或平台已经可用。

## 按任务读取

| 任务 | 指南 |
| --- | --- |
| 初始化、依赖、版本、构建、反射和热重载 | [项目与构建](references/project-build.md) |
| 原生类、Model、System、MVP、App 生命周期 | [架构与所有权](references/architecture.md) |
| Event、Reactive、协程、Tween、任务与主线程 | [事件与异步](references/events-async.md) |
| 输入、存档、资源、场景、网络、下载、音频、时间、池 | [游戏服务](references/game-services.md) |
| 原生 UI、自适应、列表、绑定、Atlas、MultiPatch | [界面与资源工具](references/ui-assets.md) |
| C++ 关卡编辑器、编辑器插件、测试代码打包隔离 | [编辑器与构建边界](references/editor-tools.md) |
| 测试、泄漏、崩溃、性能、MCP、导出与交付 | [验证与交付](references/validation-export.md) |
| 框架创建器、依赖解析、codegen 与插件维护 | [框架维护](references/framework-maintenance.md) |
| 可核对的业务、UI、迁移、任务、资源和编辑器示例 | [典型示例](references/examples.md) |

## 始终遵守的约束

- 调用服务使用 `meowody::App::resource->` 等 Proxy；Proxy 只暴露 `operator->`。不要恢复旧 `Runtime::resources()` 接口，不在静态初始化时访问 App。
- 原生反射类使用 `GDREFLECT_CLASS`、`GDPROPERTY` 等现有宏；不要手写它们的 `_bind_methods`。不修改 `build/generated` 或绑定生成的 `.gen.*` 文件。
- Godot 对象和场景树操作留在主线程；后台任务只传普通 C++ 数据。异步回调必须核对所有者仍然存活，取消和销毁需要明确的所有者。
- 先界定 Node、RefCounted、普通 Object、RAII 连接和任务句柄的所有权。DLL 重载前释放原生回调；运行时判断不能代替编译排除。
- 只修改任务所需内容，以实际源码为准；遇到手册差异核对后修正文档。示例不是必须套用的架构，简单功能不强制引入 System 或 Presenter。

## 验证与交付习惯

先选择与改动匹配的最小验证，再按失败证据扩大范围。框架已有测试使用共享隔离工作区并串行执行；新项目先检查自身测试入口。保护用户正在运行的编辑器和未保存场景。区分“编译通过”“编辑器验证”“导出运行”“真机验证”，只报告实际完成的检查。细节见验证指南。
