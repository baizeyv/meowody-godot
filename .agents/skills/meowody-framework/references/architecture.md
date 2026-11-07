# 原生业务架构、App 与所有权

适用：业务系统、共享状态、UI 组织、退出流程和对象释放。手册：#app、#application-lifecycle、#mvp、#ui、#core。示例见 [典型示例](examples.md)。

## 模型、业务和视图

源码：`src/runtime/system/system.h`、`system_center.h`、`src/runtime/save/model_fields.h`、`src/runtime/ui/ui_presenter.h`、`ui_center.h`。

Model 保存状态；System 修改状态和执行规则；View 展示和暴露输入；Presenter 连接两者。普通业务类不需要变成 Godot Object，也不需要添加 App 字段。简单界面可直接用 View 钩子；只有出现共享状态或复杂订阅时才引入 MVP。

- 可存档模型继承非模板 `Model`，`save_fields()` 返回 `std::vector<ModelField>`，使用 `model_field(name, field)`。字段引用不拥有模型，不能在模型销毁后保留。
- `App::system->register_system<T>(args...)` 或 `register_systems<T...>()` 注册普通 C++ 对象，随后显式 `initialize()`。`get<T>()` 只查询，不触发初始化。
- 需要生命周期时继承可选 `System`：`initialize()` 返回 `SystemStatus`，`shutdown() noexcept` 也必须能清理部分初始化。失败会清理当前及此前已启动系统，逆序关闭并允许重试。
- 只能在收集阶段新增类型；初始化/关闭回调中不能注册，运行后也不能新增。`shutdown()` 每实例只关闭一次，即使外部 shared_ptr 仍存活；`clear()` 释放注册并开始新周期。系统没有隐式每帧更新。
- `UIPresenter` 非模板，工厂是 `UIPresenterFactory`。`track(Connection)` 接管 RAII 订阅，`connect()` 管理 Godot 信号，`defer()` 逆序执行清理。`disconnect()` 幂等且不可逆。
- Presenter 每次重新挂载都会新建；缓存的是 View 节点树，因此 `_ready()` 不一定重跑。配置 Presenter 后，不会自动再调用 View 同名生命周期；需要时由 Presenter 显式转发。
- Presenter 的关闭不应停止整个游戏共享任务；工厂捕获 shared_ptr 会延长 System 生命周期，取消职责按真实任务所有者决定。

## App 与应用生命周期

源码：`src/runtime/app.h`、`app.cpp`、`app_lifecycle.cpp`、`application_lifecycle.h`。App 驱动各服务和延迟 tween/coroutine/save 阶段，不再自己重复 update。App 关闭 UI 后关闭业务 Systems，依赖服务随后清理。

焦点丢失、系统挂起、最小化、手动游戏暂停是不同状态。桌面失焦仅抑制输入；实际后台/最小化暂停游戏时钟和服务，恢复首帧不推进游戏时间。忽略 headless 的合成最小化。不要劫持编辑器生命周期。

通过 App 实例的 `set_game_paused()`、`request_back()`、`request_quit()`、`quit()` 处理生命周期；不要绕过它直接 pause/quit SceneTree。业务用 `ApplicationEvent` 在同步 flush 前形成 Model 快照，处理 `ApplicationSaveFailed`；保存失败时正常退出可以失败，强制退出必须是显式选择。挂起层不覆盖音频/tween/下载的手动暂停；新下载也必须遵守后台状态。低内存只清理空闲池、缓存 UI 和 LRU，不能清除活动对象。

## 释放责任

| 对象 | 通常的所有者与清理 |
| --- | --- |
| 已挂树 Node | SceneTree/父节点；脱离父节点不等于释放，必要时 queue_free |
| 未挂树 Node | 创建者；失败路径也要释放 |
| RefCounted/Resource | `godot::Ref<T>`，避免手工 free |
| 普通 Godot Object，如 UndoRedo | 创建者显式释放；面板销毁不会自动释放其非 Node 成员 |
| 普通 C++ 对象 | 栈、unique_ptr、shared_ptr；不要用 Godot Ref 管理 |
| Connection/InputContext | 持有 move-only token 的业务所有者；析构/显式断开 |
| 原生 Callable、信号连接 | DLL 卸载前断开，不能留给引擎对象继续调用 |

每个跨帧回调说明谁持有、何时取消、如何验证目标存活。Godot 节点用 ObjectID 核验；普通 C++ 对象用 weak_ptr。不能仅凭回调里捕获了裸 this 推断安全。

验证：系统初始化失败清理及逆序关闭、UI 反复开关无重复订阅、退出存档失败路径；对象泄漏用 `--verbose` 查实际类型，不压制警告。
