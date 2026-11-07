# 游戏服务选择与使用边界

适用：业务功能调用框架服务。先读相关头文件和手册对应锚点，以下是决策约束，不替代完整签名。

| 能力 | 手册 | 源码（项目根相对路径） | 关键规则 |
| --- | --- | --- | --- |
| 输入 | #input | `src/runtime/input/input_center.h`、`input_bindings.cpp` | 持有 move-only InputContext；优先级/最新激活优先，exclusive 屏蔽下层，additive 声明使用的动作。焦点/上下文/绑定变化当帧抑制边沿；不拦截 Godot Control 原始输入 |
| 日历与校时 | #time | `src/runtime/time/time_center.h` | 区分真实日历时间和游戏时间；每日订阅保存连接，测试跨日/校时，不以帧累计假装日历 |
| 音频 | #audio | `src/runtime/audio/audio_center.h` | 使用现有通道及 fade 能力；常驻音乐不随场景销毁，避免业务和生命周期重复覆盖暂停状态 |
| HTTP | #net | `src/runtime/net/net_center.h` | 使用实际请求/取消句柄；可达性监测不代表某次请求必成功，处理业务响应与网络失败 |
| 下载 | #download | `src/runtime/download/download_center.h` | 校验断点续传、文件身份、取消与缓存；挂起及时关闭文件/socket，恢复核对 Range 响应 |
| 存档 | #save | `src/runtime/save/save_center.h`、`model_fields.h` | 读取失败保留原数据，不用默认值覆盖损坏或更高版本存档；写入可能延迟，关键节点检查 flush 状态 |
| 对象池 | #pool | `src/runtime/pool/pool_center.h` | 取出重置状态/订阅，归还停止工作；低内存仅清理空闲对象，不释放仍使用的节点 |
| 资源 | #resource | `src/runtime/resource/resource_center.h`、`load_handle.h` | 通过已配置地址加载，不直接传任意文件路径/URL；保留句柄和使用中的 Ref |
| 场景 | #scene | `src/runtime/scene/scene_center.h` | `switch_to(address)`，按句柄处理失败/取消；不要同时使用另一套 SceneTree 切场景流程 |
| UI | #ui、#mvp | `src/runtime/ui/ui_center.h`、`ui_presenter.h` | 配置地址与可选 Presenter 工厂；关闭时解除 UI 订阅，缓存节点不缓存旧 Presenter |

## 存档与迁移

`Model::save_version()` 默认 0；无版本字段的旧存档按 v0 处理。`migrate_save(from, Dictionary&)` 每次只推进一个版本，仅修改传入深拷贝快照。`MODEL_VERSION_KEY` 保留，不能作为业务字段使用。

`SaveCenter::load_model(part, slot, Model&)` 返回 `ModelLoadResult`，全量校验后才更新字段与 Reactive 通知。新版本、迁移失败、类型错误都不能触发保存默认值。成功迁移不会自动重写文件，由业务在确认成功后显式 save/flush。Dictionary 和 Resource 存取不会自动使用 Model 迁移。

SaveCenter 公开读取结果必须是深拷贝；业务不能借返回值修改内部待写快照。配置加密前读取 SaveOptions 的密钥长度和限制，不能把校验和当加密。持久化操作失败要保留待写状态并向业务报告。迁移示例见 [典型示例](examples.md)。

## 资源和场景

App 启动会发起 ResourceCenter 初始化；业务加载前检查 `is_initialized()`，需要独立初始化时保留并等待 `ResourceOperation`。不要对 App 已在初始化的中心反复 initialize。

资源设置在 Godot 根的 `resources/settings.json`。Source 模式解析 UID，Packed 模式选定兼容平台目录后下载、校验依赖包并挂载。使用资源后不能随意切目录/模式，挂载失败不在同一进程中盲目重试。`pack_url`、`switch_pack` 已移除。

LoadHandle 拷贝共享同一请求；一份 release 会影响所有拷贝。独立请求的取消互不影响；默认 LRU 容量为 0，实际生命周期由句柄、分组和业务 Ref 决定。释放旧分组应在确认新场景成功并且不再需要旧资源之后。

SceneCenter 提交前取消可保留旧场景，提交后不做回滚；重型节点实例化和 `_ready()` 仍在主线程，淡入淡出不保证无卡顿。UI 或场景回调捕获的节点要检查生存期。

验证：按功能选择缺失/损坏/高版本存档、绑定重映射、HTTP 失败/取消、下载恢复、资源依赖失败、场景重复切换及 UI 关闭路径。只运行项目实际提供的测试；平台真机覆盖不能由 Windows 模拟替代。
