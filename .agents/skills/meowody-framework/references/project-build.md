# 项目识别、依赖与构建

适用：接手项目、编译失败、Godot 版本切换、添加原生类、反射或 DLL 重载。手册：#quickstart、#layout、#build-options、#reflection、#binding。

## 路径与版本

| 名称 | 识别方式 |
| --- | --- |
| 项目根 | `SConstruct` 与 `src/runtime/app.h` |
| Godot 根 | `meowody-project.json` 的 `project_dir`，未配置默认 `meow/`；创建器通常生成 `project/` |
| 手册 | 根目录 `FRAMEWORK.md` 优先，否则 `README.md` |
| 插件 | Godot 根下 `addons/`；引擎里的 `res://` 也相对此根 |
| C++ include | 根目录 `src/`；不使用资源路径 |

检查 `framework-dependencies.json`、`.gitmodules` 与 godot-cpp gitlink 是否指向同一已批准版本；克隆后在根目录执行 `git submodule update --init --recursive`。不要把独立版本的 godot-cpp 标签强行解释成 Godot 同名版本，也不要自动追踪其 main 分支。

读取 `SConstruct` 的实际参数优先级：显式 `api_version` 优先于项目配置；存在 `custom_api_file` 时检查文件和目标引擎对应。升级时重新生成 API/反射并重编译目标 DLL，不能复用其他 API 的 DLL。引擎位置从实际安装、当前编辑器或用户配置发现，用 `--version` 核实，不写死机器路径。

## 构建

根目录常用命令：`scons`；正式构建：`scons target=template_release`。需要平台、架构、API 或优化参数时先查看当前 SConstruct/godot-cpp 参数，不能凭旧版本经验拼参数。保留 RTTI：godot-cpp 的 `Wrapped::_postinitialize()` 使用 `dynamic_cast`，Release 也需要。不要用强制体积优化覆盖用户选择的 `optimize`。

普通框架构建不固定 `-j 4/-j 8`。输出写入 `build/` 日志，等待构建结束，再读取错误和最终结果。用户编辑器可能占用 DLL，先判断实际锁定进程；不要关闭其他项目或丢弃未保存场景。当前热重载能否处理新类型要实际验证，新注册类可能需要重新打开项目。

Python 只属于现有 SCons、反射和测试开发工具链；游戏/创建器功能不要因此依赖额外 Python 进程。

## 反射与绑定

源码入口：`src/reflection/`、`reflect_codegen/`、`src/register_types.cpp`、`src/editor/binding/` 和 `addons/meowody_binding/`。

- 普通 Godot 原生类按现有类使用 `GDREFLECT_CLASS`；字段通过 `GDPROPERTY`，方法通过 `GDMETHOD` 等已存在宏。检查真正的宏签名及附近示例，不发明元数据。
- codegen 自动补齐访问器和绑定；已有业务 setter 可能执行校验、信号和资源通知，必须保留。生成访问器调用写到 cpp 实现，扫描头文件时生成声明可能尚不可见。
- 绑定用户文件 h/cpp 可维护，`includes.gen.h`、`bindings.gen.inc` 与 `build/generated/` 由工具管理。原生节点按类分 h/cpp，着色器留在独立 `.gdshader` 资源。
- codegen 扫描的是包含完整函数体的单个未保存翻译单元；保留原始头文件诊断，不通过关闭诊断或跳过普通头文件掩盖错误。
- 修改源码过滤规则时同时检查反射头扫描、生成注册、初始化调用和链接依赖；仅删一个 cpp 不足以排除一个反射类。

验证：普通类至少编译；反射类验证 ClassDB 注册和属性；绑定/热重载修改使用项目已有相应驱动。没有测试驱动的新项目，添加最小项目自有探针，不调用框架仓库专用命令。
