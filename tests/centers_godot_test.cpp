#include "runtime/input/input_center.h"
#include "runtime/task/task_center.h"
#include "runtime/save/save_center.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <chrono>

using namespace godot;
using namespace meowody;
class MeowodyCentersProbe : public RefCounted {
    GDCLASS(MeowodyCentersProbe, RefCounted)
    InputCenter input;
    InputContext game, menu, overlay;
    MainThreadCenter main;
    TaskCenter tasks{main, 1};
    TaskHandle task;
    bool skipped_dead_owner = false;
protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("phase", "index"), &MeowodyCentersProbe::phase); }
public:
    bool phase(int index) {
        bool okay = true;
        auto check = [&](bool passed, const char *message) { if (!passed) { okay = false; UtilityFunctions::printerr("[CentersTest] FAIL: ", message); } };
        auto map = InputMap::get_singleton(); auto raw = Input::get_singleton();
        if (index == 0) {
            for (const char *name : {"mdy_move", "mdy_menu", "mdy_left", "mdy_right", "mdy_up", "mdy_down"}) map->add_action(name, 0.2f);
            Ref<InputEventKey> key; key.instantiate(); key->set_keycode(KEY_W); map->action_add_event("mdy_move", key);
            PackedStringArray actions;
            for (const char *name : {"mdy_move", "mdy_left", "mdy_right", "mdy_up", "mdy_down"}) actions.append(name);
            check(input.define_context("game", actions, 0, false), "game definition");
            PackedStringArray menus; menus.append("mdy_menu");
            check(input.define_context("menu", menus, 10, true), "menu definition");
            PackedStringArray overlays; overlays.append("mdy_move");
            check(input.define_context("overlay", overlays, 20, false), "overlay definition");
            game = input.push_context("game"); check(game.valid(), "context valid");
            raw->action_press("mdy_move"); raw->action_press("mdy_right"); raw->action_press("mdy_up");
            check(input.is_action_pressed("game", "mdy_move"), "pressed poll");
            check(!input.is_action_just_pressed("game", "mdy_move"), "activation suppresses edge");
            Vector2 vector = input.get_vector("game", "mdy_left", "mdy_right", "mdy_up", "mdy_down");
            check(vector.x > 0 && vector.y < 0 && vector.length() <= 1.001, "radial vector normalization");
            auto node = memnew(Node); const uint64_t object_id = node->get_instance_id(); memdelete(node);
            task = tasks.submit([](const TaskContext &) { return TaskResult{TaskError::None, int64_t(42), {}}; },
                [this, object_id](const TaskResult &result) {
                    skipped_dead_owner = result.ok() && ObjectDB::get_instance(object_id) == nullptr;
                });
        } else if (index == 1) {
            raw->action_release("mdy_move"); raw->action_press("mdy_move");
            check(input.is_action_just_pressed("game", "mdy_move"), "fresh edge after activation frame");
            menu = input.push_context("menu");
            check(!input.is_action_pressed("game", "mdy_move") && raw->is_action_pressed("mdy_move"), "exclusive menu gates framework only");
            check(menu.set_enabled(false) && input.is_action_pressed("game", "mdy_move"), "disable context");
            menu.set_enabled(true); menu.release();
            check(!input.is_action_just_pressed("game", "mdy_move"), "resume does not replay edge");
            overlay = input.push_context("overlay");
            check(!input.is_action_pressed("game", "mdy_move") && input.is_action_pressed("game", "mdy_right"), "additive overlay consumes matching action only");
            check(input.is_action_pressed("overlay", "mdy_move"), "overlay reads claimed action");
            auto duplicate_game = input.push_context("game");
            check(duplicate_game.valid() && !input.is_action_pressed("game", "mdy_move"), "later low-priority activation remains masked");
            check(input.is_action_pressed("overlay", "mdy_move"), "higher priority survives later low-priority activation");
            duplicate_game.release();
            PackedStringArray peer_actions; peer_actions.append("mdy_move");
            check(input.define_context("peer", peer_actions, 0, false), "equal-priority peer definition");
            overlay.release();
            auto peer = input.push_context("peer");
            check(!input.is_action_pressed("game", "mdy_move") && input.is_action_pressed("peer", "mdy_move"), "latest equal-priority activation wins");
            duplicate_game = input.push_context("game");
            check(input.is_action_pressed("game", "mdy_move") && !input.is_action_pressed("peer", "mdy_move"), "latest same-name activation becomes target");
            duplicate_game.set_enabled(false);
            check(!input.is_action_pressed("game", "mdy_move") && input.is_action_pressed("peer", "mdy_move"), "disabled latest activation falls back to previous target");
            duplicate_game.release(); peer.release();
            input.set_focused(false);
            check(!input.is_action_pressed("game", "mdy_move"), "focus out");
            input.set_focused(true); input.set_enabled(false);
            check(input.get_axis("game", "mdy_left", "mdy_right") == 0, "global disable");
            input.set_enabled(true);
            check(!input.is_action_just_pressed("game", "mdy_move"), "focus resume edge suppression");
        } else if (index == 2) {
            TypedArray<InputEvent> events;
            Ref<InputEventKey> key; key.instantiate(); key->set_keycode(KEY_J); events.append(key);
            Ref<InputEventMouseButton> mouse; mouse.instantiate(); mouse->set_button_index(MOUSE_BUTTON_LEFT); mouse->set_shift_pressed(true); events.append(mouse);
            Ref<InputEventJoypadButton> button; button.instantiate(); button->set_device(-1); button->set_button_index(JOY_BUTTON_A); events.append(button);
            Ref<InputEventJoypadMotion> axis; axis.instantiate(); axis->set_device(-1); axis->set_axis(JOY_AXIS_LEFT_X); axis->set_axis_value(-1); events.append(axis);
            check(input.rebind("mdy_move", events, 0.3f).ok(), "all binding kinds");
            PackedStringArray actions; actions.append("mdy_move"); actions.append("mdy_right");
            auto exported = input.export_bindings(actions); check(exported.ok(), "binding export");
            SaveCenter saves("user://input_test");
            check(saves.save("input", "default", exported.data).ok() && saves.flush_pending().ok(), "binding persistence");
            check(input.reset_bindings("mdy_move"), "default reset");
            Ref<InputEventKey> default_key = map->action_get_events("mdy_move")[0];
            check(default_key->get_keycode() == KEY_W, "default key W");
            check(input.import_bindings(saves.load("input", "default").data).ok(), "binding restore");
            auto restored = map->action_get_events("mdy_move");
            Ref<InputEventKey> restored_key = restored[0]; Ref<InputEventMouseButton> restored_mouse = restored[1];
            Ref<InputEventJoypadButton> restored_button = restored[2]; Ref<InputEventJoypadMotion> restored_axis = restored[3];
            check(restored.size() == 4 && restored_key->get_keycode() == KEY_J && restored_mouse->is_shift_pressed(), "key/modifier roundtrip");
            check(restored_button->get_device() == -1 && restored_axis->get_axis_value() == -1, "joypad roundtrip");
            Dictionary malformed = exported.data.duplicate(true); Dictionary bindings = malformed["actions"];
            Dictionary right = bindings["mdy_right"]; right["deadzone"] = 1.0;
            Dictionary move = bindings["mdy_move"]; Array records = move["events"]; Dictionary record = records[0]; record["keycode"] = int(KEY_K);
            check(!input.import_bindings(malformed).ok(), "reject malformed batch");
            restored_key = map->action_get_events("mdy_move")[0]; check(restored_key->get_keycode() == KEY_J, "import failure is atomic");
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!main.pending_count() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            main.update(128, 0); check(skipped_dead_owner && task.status() == TaskStatus::Completed, "worker result with freed Godot owner");
        } else if (index == 3) {
            raw->action_release("mdy_move");
            Ref<InputEventKey> key; key.instantiate(); key->set_keycode(KEY_J); key->set_pressed(true);
            raw->parse_input_event(key); raw->flush_buffered_events();
            check(input.is_action_pressed("game", "mdy_move"), "real mapped key event");
            Ref<InputEventKey> released; released.instantiate(); released->set_keycode(KEY_J); released->set_pressed(false);
            raw->parse_input_event(released); raw->flush_buffered_events();
            check(!input.is_action_pressed("game", "mdy_move"), "mapped key release");
            tasks.shutdown(); main.shutdown(); input.shutdown();
            check(!game.valid() && !input.push_context("game").valid(), "closed input invalidates contexts");
        }
        return okay;
    }
};
static void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<MeowodyCentersProbe>();
}
extern "C" {
GDExtensionBool GDE_EXPORT centers_test_init(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization *init) {
    GDExtensionBinding::InitObject object(proc, library, init);
    object.register_initializer(initialize); object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE); return object.init();
}
}
