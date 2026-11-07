# 事件、响应式、协程与异步任务

适用：事件交互、UI 绑定、定时流程、动画、后台计算和取消。手册：#core、#event、#reactive、#coroutine、#tween、#main-thread、#task。

| 能力 | 源码入口 | 选择依据与约束 |
| --- | --- | --- |
| Connection、Delegate、TypeToken | `src/runtime/core/` | 保留返回的 RAII 连接；临时连接离开语句就会断开。普通回调不一定需要 Godot Callable |
| EventCenter | `src/runtime/event/` | 类型事件，读实际 connect/emit 重载；回调可修改连接，新增订阅遵守当前派发边界 |
| Reactive、Computed、Effect | `src/runtime/reactive/` | 用于可观察状态和派生值；保留依赖/Effect 生命周期，不为了显示单一静态值引入响应式图 |
| CoroutineCenter | `src/runtime/coroutine/` | 主线程跨帧流程，不是后台线程；取消、等待、恢复顺序查当前 awaiter 与手册 |
| TweenCenter | `src/runtime/tween/` | 值补间与 Sequence；明确所有者、暂停与取消，避免结束回调访问已销毁节点 |
| MainThreadCenter | `src/runtime/main_thread/` | `post()` 可跨线程；update/shutdown/destruction 属于创建线程 |
| TaskCenter | `src/runtime/task/` | 耗时普通数据计算；工作线程不碰场景树与 Godot Variant/JSON |

## 任务数据与生命周期

`TaskCenter::submit(work, completed)` 返回 TaskHandle。工作接收 `const TaskContext&`，检查 `is_cancelled()`，返回显式 `TaskResult`；失败使用 `TaskError`。`TaskValue` 是固定 std::variant，支持普通字符串、字节数组和约定的 shared_ptr<void> 数据；不要往里塞 Godot 对象。异常转换只在启用异常时可用，Release 不应依靠 throw 传递业务错误。

完成回调由主线程交付。句柄释放不等于任务取消；所有者销毁时明确 cancel。取消是合作式，不会强杀正在执行的函数。回调捕获 weak_ptr/ObjectID，不跨线程捕获裸节点。大型工作拆分取消检查点；不要在主线程等待一个依赖主线程交付的任务。

TaskCenter shutdown 取消并等待工作线程；App 随后丢弃尚未执行的主线程投递，业务不能依赖退出期间一定收到完成回调。独立持有的中心必须按 tasks.shutdown → main.shutdown 的顺序清理；App 持有的中心由 App 负责，不重复 shutdown/update。

## 响应式和派发边界

Reactive 通过 set/赋值修改，get 读取；查看 `reactive_value.hpp` 中订阅是否立即回调及其参数再使用。Source/Dependency 不能在自己正在执行的通知中销毁。允许回调自断开，但不能因此把正在执行的槽位提前复用。Computed/Effect 的依赖收集遵守框架规则，不在不同线程修改同一响应式对象。

维护 Event/coroutine 时保留注册序号和派发边界；维护 Reactive 时保留 Callable 地址稳定性及执行期间槽位保护。只有性能改动涉及这些内部结构时才读取相应基准，不为普通业务改写中心。

验证：回调主线程身份、运行中取消、所有者提前销毁后不再更新、回调自断开、新订阅下一轮生效。框架存在时使用 `tests/async_centers_test.cpp`、`tests/run_centers_test.py` 或相关响应式/协程测试；新项目不假定这些文件已分发。可取消计算示例见 [典型示例](examples.md)。
