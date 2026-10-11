#include "runtime/app.h"

void snippet_0() {
#include "runtime/app.h"

// 普通 C++ 回调，可从工作线程投递；回调在 App 的主线程执行。
auto pending = meowody::App::main_thread->post([] {
    // 这里可以更新 Model 或调用 Godot 节点 API。
});
pending.cancel(); // 仅能取消尚未开始执行的回调。

}

#include "runtime/app.h"
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/object.hpp>

meowody::DispatchHandle post_label_text(uint64_t label_id, std::string text) {
    return meowody::App::main_thread->post([label_id, text = std::move(text)] {
        auto *label = godot::Object::cast_to<godot::Label>(godot::ObjectDB::get_instance(label_id));
        if (label) label->set_text(godot::String(text.c_str()));
    });
}

#include "runtime/app.h"

meowody::TaskHandle calculate_sum(std::vector<int64_t> numbers) {
    return meowody::App::task->submit(
        [numbers = std::move(numbers)](const meowody::TaskContext &context) -> meowody::TaskResult {
            double sum = 0;
            for (auto value : numbers) {
                if (context.is_cancelled())
                    return {meowody::TaskError::Cancelled, {}, "Cancelled"};
                sum += double(value);
            }
            return {meowody::TaskError::None, sum, {}};
        },
        [](const meowody::TaskResult &result) {
            if (!result.ok()) return; // 记录 result.message 或显示失败状态。
            const auto *sum = std::get_if<double>(&result.value);
            if (!sum) return;
            // 在主线程使用 *sum 更新 Model／UI；不可保存此临时引用。
        });
}

void snippet_3() {
meowody::TaskHandle task = calculate_sum({10, 20, 30});
// Presenter 已配置生命周期时，可登记面板级取消：
// presenter.defer([task] { task.cancel(); });
task.cancel();

}

struct ComputedRows { std::vector<int64_t> scores; };

meowody::TaskResult make_rows(const meowody::TaskContext &context) {
    if (context.is_cancelled()) return {meowody::TaskError::Cancelled, {}, "Cancelled"};
    auto rows = std::make_shared<ComputedRows>();
    rows->scores = {900, 800, 700};
    std::shared_ptr<void> payload = rows;
    return {meowody::TaskError::None, std::move(payload), {}};
}
void receive_rows(const meowody::TaskResult &result) {
    if (!result.ok()) return;
    const auto *payload = std::get_if<std::shared_ptr<void>>(&result.value);
    if (!payload || !*payload) return;
    // 必须与对应工作函数返回的实际类型一致。
    auto rows = std::static_pointer_cast<ComputedRows>(*payload);
    // 主线程使用 rows->scores。
}

#include "runtime/app.h"

class GameInput {
    meowody::InputContext gameplay;
    meowody::InputContext menu;
public:
    bool initialize() {
        godot::PackedStringArray game_actions;
        for (const char *name : {"move_left", "move_right", "move_up", "move_down", "interact"}) game_actions.append(name);
        godot::PackedStringArray menu_actions;
        menu_actions.append("menu_back");
        if (!meowody::App::input->define_context("gameplay", game_actions, 0, false) ||
            !meowody::App::input->define_context("menu", menu_actions, 100, true)) return false;
        gameplay = meowody::App::input->push_context("gameplay");
        return gameplay.valid();
    }
    void open_menu() { menu = meowody::App::input->push_context("menu"); }
    void close_menu() { menu.release(); }
    godot::Vector2 movement() const {
        return meowody::App::input->get_vector("gameplay", "move_left", "move_right", "move_up", "move_down");
    }
    bool interact() const { return meowody::App::input->is_action_just_pressed("gameplay", "interact"); }
    bool menu_back() const { return meowody::App::input->is_action_just_pressed("menu", "menu_back"); }
    void shutdown() { menu.release(); gameplay.release(); }
};

#include "runtime/app.h"
#include <godot_cpp/classes/input_event_key.hpp>

meowody::SaveStatus bind_interact_to_e() {
    godot::Ref<godot::InputEventKey> key;
    key.instantiate(); key->set_physical_keycode(godot::KEY_E);
    godot::TypedArray<godot::InputEvent> events;
    events.append(key);
    return meowody::App::input->rebind("interact", events);
}

#include "runtime/app.h"

meowody::SaveStatus save_input_bindings() {
    godot::PackedStringArray actions;
    actions.append("interact"); actions.append("move_left");
    auto exported = meowody::App::input->export_bindings(actions);
    if (!exported.ok()) return {exported.error, exported.message};
    return meowody::App::save->save("input", "default", exported.data);
}
meowody::SaveStatus restore_input_bindings() {
    auto loaded = meowody::App::save->load("input", "default");
    if (!loaded.ok()) return {loaded.error, loaded.message};
    return meowody::App::input->import_bindings(loaded.data);
}
