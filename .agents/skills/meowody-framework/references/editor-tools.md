# 关卡编辑器、插件与打包边界

适用：在同一个 C++ 工程中开发关卡编辑器、工具和测试，但导出的游戏不携带它们。源码：根目录 `SConstruct`、`reflect_codegen/SConscript`、`src/register_types.cpp`、`src/editor_registration.h`、`src/editor_registration_stub.cpp`，Godot 根下 `bin/meowody.gdextension`。

## 当前已经支持和未支持的行为

当前 SConstruct 收集 src 下 cpp，Release 排除 `src/editor/`，并链接编辑注册 stub；普通 Debug 仍包含编辑器源码。根目录 tests 不在默认 src 收集范围。反射生成仍扫描 src 下头文件。因此，**当前不是 Editor / Debug 游戏 / Release 游戏三种完全隔离构建**，不能仅将反射类移入 src/editor 就承诺导出已排除。

`MODULE_INITIALIZATION_LEVEL_EDITOR` 限制初始化时机；`Engine::is_editor_hint()` 限制运行行为；二者均不会从 DLL 删除代码。DEBUG_ENABLED 也不能单独代表“仅编辑器”，Debug 游戏同样可能定义它。导出器过滤 tscn、图片或插件目录不会移除 DLL 中已编译的 C++。

## 新项目组织建议

| 内容 | 建议放置 | 边界 |
| --- | --- | --- |
| 游戏加载器、关卡数据 Resource、运行时对象 | `src/game/` | 游戏运行必须存在，不能依赖编辑器类 |
| 编辑面板、笔刷、选区、导入/保存工具 | `src/editor/` | 编辑器专用；遵守现有构建限制 |
| 自动化测试、测试入口和测试构建文件 | 根目录 `tests/` | 独立测试目标，不注册到游戏正常入口 |
| 最终关卡及运行时素材 | Godot 根下产品资源目录 | 随游戏按依赖导出 |
| 编辑器临时资源、测试场景和图片 | 明确的开发目录 | 由导出预设或导出插件显式排除，并实际检查包内容 |

这是组织约定，不会自动创建新构建模式。普通短期 C++ 后端可配合 GDScript EditorPlugin；如果必须纯 C++ 编辑器 UI，需先明确 DLL 重载策略，避免保留引擎对象上的原生回调。

## 用户要求真正隔离时的实施顺序

1. 明确是仅正式 Release 排除，还是 Debug 导出也排除。后者需要单独 Editor 动态库和游戏 Debug/Release 动态库；用 .gdextension editor/debug/release feature keys 选择，并验证匹配优先级和导出文件。
2. 同时调整源码选择、反射头扫描/生成注册、编辑初始化与链接 stub；输出目录及生成物按配置隔离，不能共享一份互相覆盖的注册代码。
3. 运行时代码不 include/继承/实例化编辑器专用类，运行时场景和关卡资源不引用被排除类型。编辑器工具只生成运行时可识别的数据。
4. 独立测试目标按需编译测试代码；确认插件脚本、测试素材和编辑器专用库没有被依赖链或导出过滤再次带入。

本 skill 提供此实施路径，不自动改变当前构建体系。不要为了排除工具改写游戏所需 Resource 的注册。

## 生命周期

EditorPlugin 启用时添加面板/inspector 并连接信号，停用时移除、断开并释放自有对象。不要 free 编辑器提供的 UndoRedo manager；自行 new 的 UndoRedo 属于普通 Object，必须显式释放。对可能脱离树后销毁的面板，PREDELETE 比 EXIT_TREE 更准确表示最终释放时机；退出树不一定意味着对象不会再次使用。

原生后端对象和 Callable 必须在 DLL 卸载前清理；编辑器中跨重载存活的状态放到 GDScript。具体所有权示例见 [典型示例](examples.md)。

验证：工具启用/停用/重复打开，撤销重做、取消和退出，生成的关卡在无插件游戏中可加载；检查 Debug 与 Release 实际 DLL/资源包，不以“工具没有出现”证明没有打包。退出用 verbose 检查 ObjectDB。
