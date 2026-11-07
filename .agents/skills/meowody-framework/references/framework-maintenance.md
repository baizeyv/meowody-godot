# 框架维护专用流程

适用：修改框架创建器、依赖解析、反射生成器和插件内部逻辑。普通生成游戏不自动进入这些流程。手册：#project-creator、#reflection、#binding、#build-options、#project-skill。

## 创建器和依赖

存在 `src/project_creator/` 时，Windows x64 创建器由原生 C++ view/backend/template 实现，Godot 场景位于 Godot 根的 `project_creator/main.tscn`。工作通过 App::task 传普通数据、主线程处理 Godot JSON，偏好设置用 App::save，忙碌退出使用 ApplicationRequest。使用 WinHTTP/BCrypt 和系统 tar，无 Python/Tkinter/PyInstaller 产品助手。

保留产品现有 Catppuccin Mocha、初始 1920×1080、stretch disabled、侧栏收起和窄屏堆叠、底部动作可滚动到达、弹窗随窗口夹取的行为。checkbox 各状态边距相同；FileDialog 是嵌入的主题弹窗带遮罩；工作台换行且只垂直滚动。框架分支/标签和经过验证的本地 Godot 都是手选下拉项；下载模式自动查官方稳定版。联网查找失败时保留已有选择，不伪造最新版本。

godot-cpp 解析优先官方精确/次版本 Godot release tag，并处理 annotated tag 的 peeled commit。独立版本（例如 v10）仅按框架依赖记录批准的 API 映射选择。旧 commit-only 记录只允许已知 10.0.0-stable 对测试过的 4.3–4.7 API 的恢复，且必须核对官方稳定 tag commit；不能绕过显式限制或接受未知哈希/API。远程查询失败、版本无效和映射缺失分别报告。

生成 metadata 和 dependency manifest 记录解析后的 tag/commit；源框架 gitlink 要对照源记录核验，不能对照替换后的目标版本。生成项目必须有自己的 Git 仓库、.gitmodules 和固定 godot-cpp gitlink；忽略路径可按需 force-add，不自动 commit/push。不保留旧 branch=4.3 之类错误追踪配置。下载引擎使用官方稳定版本及 SHA512 校验；工具链检查不等于获准安装系统依赖。

## 模板与 skill 分发

生成器位于 `src/project_creator/project_template.cpp`。生成项目默认 `project/`，复制框架服务、原生编辑器后端、插件与空白场景；排除创建器业务、旧示例、TestNode、缓存和编译产物。根手册转为 FRAMEWORK.md，生成项目 README 仅说明项目入口/构建/skill 使用。

完整递归复制 `.agents/skills/meowody-framework/`，包括 references 和 agents。skill 自行按 metadata 和文件识别布局，不对 skill 文件做全局 meow/project 字符替换，不分发本机 Codex 配置。旧项目更新先查看其本地改动，不自动覆盖。新项目默认没有框架 tests；skill 指令必须先判断入口是否存在。

模板修改用 `tests/run_project_creator_test.py` 验证原生生成、文件分发、依赖初始化和实际构建/运行；需要时运行 native backend、UI 或导出驱动。测试仍使用单个共享工作区，生成 framework root 下唯一 Godot 工程为 project/。导出必须按预设的实际 PCK 模式验证，并启动官方 Debug/Release 模板。

## 内部模块维护

修改 codegen 保留单一完整翻译单元、头文件原始诊断和已有访问器语义；先跑最小 reflect 回归。修改 App 初始化/关闭顺序时验证部分失败清理、UI/System/任务/主线程的所有权。

修改插件时验证重复启停、导入退出、热重载和错误弹窗；GDScript 可以长存，native 实例与回调不能跨 DLL 卸载。不要把某次创建器 UI 偏好变成所有业务项目的硬性规范。性能内部不变量与测试见各主题指南，手册和 skill 必须与真实源码同步。
