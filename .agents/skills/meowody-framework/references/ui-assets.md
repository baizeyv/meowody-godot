# 原生 UI、自适应与资源编辑工具

适用：原生界面、绑定、控件行为、列表、图集、资源构建。手册：#ui、#binding、#nodes、#node-examples、#atlas、#multipatch、#resource。

## 界面和绑定

先检查场景节点、Container、anchors、size flags、最小尺寸和主题；自适应布局由实际窗口尺寸驱动，不把固定坐标界面误当自适应。根据产品需要设置 stretch，不能把创建器专用的 1920×1080/Catppuccin 约定强加给所有游戏。

- 容器子项由容器布局，不同时手动写其位置。窄窗口下换行或堆叠；限制最小宽度，正文跟随可用宽度换行，工作台只在需要时保留垂直滚动。
- Popup/Dialog 限制在可视窗口内，窗口改变时重新夹取大小和位置；遮罩应正确阻挡背景输入。hover/focus/pressed 主题边距一致，避免文字跳动。
- 使用 `addons/meowody_binding/` 与 `src/editor/binding/` 的现有生成流程；以实际生成成员名为准。NativeSceneRegistration 和运行时重新绑定已经废弃。
- 编辑器 UI 默认使用 GDScript，原生后端做短期调用，不跨 DLL 重载保存后端实例或原生 Callable。绑定失败保留可关闭的进度框，成功后关闭。

## 控件选择

源码入口 `src/runtime/ui/nodes/`，对应编辑辅助 `addons/meowody_nodes/`。先搜索类头和手册的 #node-* 示例：VirtualList、SnapScrollContainer、LoopScrollContainer、SegmentProgressBar、NumberLabel、AutoSizeLabel、GradientRect、ArcProgress、ShapeRect、LongPressButton、RepeatButton、DragArea、DropArea、GestureArea。

列表复用单元必须完整覆盖显示状态并替换旧订阅；循环回中同步调整动画坐标。保持原生拖放交付、释放/隐藏/禁用/失焦时取消输入的行为。Arc 绘制要保留修剪图集的逻辑裁切，PackedArray 在 resize/COW 后不能继续使用旧指针。

原生控件析构前断开构造器/Resource 信号中的 native Callable。交给引擎 Tween 的释放回调优先使用命名引擎 Callable（如 queue_free），避免 DLL 卸载后引擎仍保留函数地址。旧 DLL 没有清理逻辑时先重启一次编辑器再验证新实现。

## Atlas、MultiPatch 与资源构建

`addons/meowody_atlas/` 管理编辑器图集；运行时资源是 native SpriteAtlas 的 `atlas.tres`，UISpriteAtlas 是编辑配置。重打包保持源纹理引用、逻辑区域与透明 margin；不要依赖每次变化的页面 UV。

`addons/meowody_multipatch/` 提供切片编辑器，运行时类位于 `src/runtime/ui/`。sidecar 引用源纹理，避免竞争图像导入器。检查切线有序、模式/权重合法、小目标尺寸以及 trim/flip/crop 行为。MultiPatch 面板自行创建的 UndoRedo 必须显式 free，编辑器全局 get_undo_redo 返回的对象则不能由面板释放。

`addons/meowody_resources/` 配置地址、分组、标签及平台资源构建。导出前先构建资源目录/包，即使 entries 为空也要验证初始化配置。不要将源模式可加载误认为导出包可加载。

验证：布局至少检查宽屏、窄屏、最低支持尺寸和弹窗打开时调整窗口；GPU 像素检查必须 visual 模式。框架的绑定驱动覆盖跨三次 DLL 替换的 LongPress/Repeat/Snap/Loop，Atlas/MultiPatch 驱动覆盖导入、重打包及运行时 PCK；调用前确认驱动存在。
