# 典型示例：业务、UI、任务、资源和编辑器

这些片段使用当前公开头文件，按顺序可组合为一个头文件；各片段在 `meowody_skill_examples` 命名空间中。示例随 skill 分发，不进入 src，也不会自动编译到游戏。按实际功能选取，不必全部复制。框架仓库的 `tests/run_framework_skill_test.py` 从本文提取 C++ 做编译，并运行所有权/迁移/取消回归；生成项目没有该测试驱动时不调用它。

## 业务 Model 与 System

入口：`src/runtime/save/model_fields.h`、`save_center.h`、`src/runtime/system/system_center.h`。这里以普通 C++ 类型保存金币，System 引用 SaveCenter，中心必须比 System 活得久。游戏中可由 App::system 注册，传入 App::save.operator->() 指向的服务；不要重复创建 App 本身。

```cpp
#include "runtime/save/model_fields.h"
#include "runtime/save/save_center.h"
#include "runtime/system/system_center.h"

namespace meowody_skill_examples {
struct ProgressModel : meowody::Model {
    meowody::Reactive<int64_t> coins{0};
    std::vector<meowody::ModelField> save_fields() override {
        return {meowody::model_field("coins", coins)};
    }
    int64_t save_version() const override { return 1; }
    meowody::SaveStatus migrate_save(int64_t from, godot::Dictionary &data) const override {
        if (from != 0)
            return {meowody::SaveError::MigrationFailed, "Unknown progress version"};
        godot::Variant old = data.get("score", int64_t(0));
        if (old.get_type() != godot::Variant::INT)
            return {meowody::SaveError::MigrationFailed, "score must be an integer"};
        data["coins"] = old;
        data.erase("score");
        return {};
    }
};

class ProgressSystem : public meowody::System {
    meowody::SaveCenter &saves;
public:
    ProgressModel model;
    explicit ProgressSystem(meowody::SaveCenter &owner) : saves(owner) {}
    meowody::SystemStatus initialize() override {
        auto loaded = saves.load_model("progress", "main", model);
        if (loaded.ok() || loaded.error == meowody::SaveError::Missing) return {};
        return {meowody::SystemError::InitializationFailed, loaded.message.utf8().get_data()};
    }
    meowody::SaveStatus persist() {
        auto saved = saves.save_model("progress", "main", model);
        return saved.ok() ? saves.flush_pending() : saved;
    }
};
}
```

SystemCenter 持有 shared_ptr，App 负责 initialize 后的正常 shutdown；这个 System 没有额外资源，所以不添加空 shutdown。load 失败不会调用 persist；迁移只修改快照，不修改源文件或外部状态。

## 显式保存迁移结果

原文件没有版本时按 v0，把 score 迁移为 coins。成功迁移不会隐式覆盖旧文件；这里提供明确选择“迁移成功后写回”的操作，真实业务应在备份/保存策略确定后调用。

```cpp
namespace meowody_skill_examples {
inline meowody::SaveStatus restore_and_commit_migration(
        meowody::SaveCenter &saves, ProgressModel &model) {
    auto loaded = saves.load_model("progress", "main", model);
    if (!loaded.ok()) return {loaded.error, loaded.message};
    if (!loaded.migrated) return {};
    auto saved = saves.save_model("progress", "main", model);
    return saved.ok() ? saves.flush_pending() : saved;
}
}
```

验收：旧数据 score=7 得到 coins=7；类型错误和高于当前的存档版本拒绝，Model 和文件不被默认值覆盖。完整规则见 [游戏服务](game-services.md)。

## Presenter 管理 UI 订阅

准备根节点为 Label 的场景并注册地址 `ui/score`（真实复杂界面可换为自己的绑定 View）。Presenter 持有业务 System，UI 关闭时 UICenter 调用 disconnect；缓存节点重开会创建新 Presenter。

```cpp
#include "runtime/ui/ui_center.h"
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/object.hpp>

namespace meowody_skill_examples {
class ScorePresenter : public meowody::UIPresenter {
    std::shared_ptr<ProgressSystem> progress;
    uint64_t label_id = 0;
    void refresh() {
        auto *label = godot::Object::cast_to<godot::Label>(godot::ObjectDB::get_instance(label_id));
        if (label) label->set_text(godot::String::num_int64(progress->model.coins.get()));
    }
public:
    explicit ScorePresenter(std::shared_ptr<ProgressSystem> system) : progress(std::move(system)) {}
    void on_attach(godot::Node &root) override {
        auto *label = godot::Object::cast_to<godot::Label>(&root);
        if (!label) return;
        label_id = label->get_instance_id();
        refresh();
        track(progress->model.coins.subscribe([this](const int64_t &) { refresh(); }));
    }
};

inline meowody::SystemStatus configure_score_ui(meowody::SystemCenter &systems,
        meowody::SaveCenter &saves, meowody::UICenter &ui) {
    auto progress = systems.register_system<ProgressSystem>(saves);
    if (!progress) return {meowody::SystemError::InvalidState, "Cannot register ProgressSystem"};
    auto started = systems.initialize();
    if (!started.ok()) return started;
    if (!ui.configure("ui/score", {}, [progress]() -> std::unique_ptr<meowody::UIPresenter> {
            return std::make_unique<ScorePresenter>(progress);
        })) return {meowody::SystemError::InitializationFailed, "UI configuration failed"};
    return {};
}
}
```

这里的工厂捕获会延长 System 寿命；System 的 SaveCenter 引用仍要求服务先创建后销毁。track 接管连接，关闭后修改 Model 不应再更新 Label。ObjectID 防止节点提前释放后访问悬空指针；它不能替代 UI 关闭时断开订阅。configure 成功后由业务调用 ui.open/open_async。

## 可取消后台任务

TaskCenter 由调用者注入，App 场景可调用 `job.start(*App::task.operator->(), state, values)`。工作只处理标准容器，完成回调在主线程写普通状态，不使用 Godot JSON/Variant。

```cpp
#include "runtime/task/task_center.h"

namespace meowody_skill_examples {
struct SumState { int64_t total = 0; };
class SumJob {
    meowody::TaskHandle task;
public:
    SumJob() = default;
    SumJob(const SumJob &) = delete;
    SumJob &operator=(const SumJob &) = delete;
    ~SumJob() { cancel(); }
    void cancel() { task.cancel(); }
    void start(meowody::TaskCenter &tasks, const std::shared_ptr<SumState> &owner,
            std::vector<int32_t> values) {
        cancel();
        task = tasks.submit(
            [values = std::move(values)](const meowody::TaskContext &context) -> meowody::TaskResult {
                int64_t total = 0;
                for (auto value : values) {
                    if (context.is_cancelled()) return {meowody::TaskError::Cancelled, {}, {}};
                    total += value;
                }
                return {meowody::TaskError::None, total, {}};
            },
            [weak = std::weak_ptr<SumState>(owner)](const meowody::TaskResult &result) {
                auto state = weak.lock();
                auto total = std::get_if<int64_t>(&result.value);
                if (state && result.ok() && total) state->total = *total;
            });
    }
};
}
```

SumJob 销毁显式取消；worker 不捕获 SumJob。weak_ptr 不延长结果接收者寿命，目标消失时跳过写入。检查队列拒绝/任务失败需要时可暴露句柄状态，示例没有加入未使用的重试机制。独立中心按 TaskCenter shutdown 后 MainThreadCenter shutdown 清理；App 持有时由 App 管理。

## 地址资源与关卡切换

在资源设置中配置 `icons/level` 为 Texture2D，`levels/first` 为 PackedScene。App 已发起初始化，业务等待 ready 后调用 begin，在主线程每帧 tick；不重复 update App 的中心。

```cpp
#include "runtime/scene/scene_center.h"
#include <godot_cpp/classes/texture2d.hpp>

namespace meowody_skill_examples {
class LevelFlow {
    meowody::LoadHandle<godot::Texture2D> icon;
    meowody::SceneHandle transition;
public:
    LevelFlow() = default;
    LevelFlow(const LevelFlow &) = delete;
    LevelFlow &operator=(const LevelFlow &) = delete;
    ~LevelFlow() { icon.release(); transition.cancel(); }
    bool begin(meowody::ResourceCenter &resources) {
        if (!resources.is_initialized() || icon.valid()) return false;
        icon = resources.load_async<godot::Texture2D>("icons/level");
        return true;
    }
    bool tick(meowody::SceneCenter &scenes) {
        if (!icon.valid() || !icon.finished()) return false;
        if (!icon.succeeded()) return false;
        if (!transition.valid()) transition = scenes.switch_to("levels/first");
        return transition.is_finished() && transition.result().succeeded;
    }
};
}
```

LevelFlow 独占自己的请求句柄；销毁时释放图标请求并尝试取消未提交的场景切换。提交后的场景归 SceneTree，cancel 不回滚。这里是单次流程：失败时返回 false，实际 UI 应读取并展示句柄错误，不无限重复切场景。成功返回只表示切换完成，不代表已完成关卡业务初始化。

## 编辑器工具的对象清理

下面是普通 C++ 辅助对象，拥有自行创建的 UndoRedo；不要把编辑器全局 get_undo_redo 的对象传入并交给它释放。它不需要反射注册。适用于生命周期受控的 native 工具，必须在 DLL 卸载前销毁。

```cpp
#include <godot_cpp/classes/undo_redo.hpp>
#include <godot_cpp/core/memory.hpp>

namespace meowody_skill_examples {
class DraftHistory {
    godot::UndoRedo *history = memnew(godot::UndoRedo);
public:
    DraftHistory() = default;
    DraftHistory(const DraftHistory &) = delete;
    DraftHistory &operator=(const DraftHistory &) = delete;
    ~DraftHistory() { memdelete(history); }
    godot::UndoRedo &get() { return *history; }
};
}
```

需要保持 DLL 热重载时，长存面板和历史状态放在 GDScript，C++ 只作短期后端调用。面板最后销毁时清理自己的普通 Object，退出树不等于最后销毁：

```gdscript
@tool
extends VBoxContainer

var undo := UndoRedo.new()

func _notification(what: int) -> void:
    if what == NOTIFICATION_PREDELETE:
        undo.free()
```

EditorPlugin 停用时先断开信号、移除底部面板，再 queue_free 自有面板；PREDELETE 负责释放 undo。工具输出运行时关卡数据，运行时代码不依赖工具类。这个例子只展示释放责任，不声称当前工程已有三种完全隔离构建，详见 [编辑器与构建边界](editor-tools.md)。
