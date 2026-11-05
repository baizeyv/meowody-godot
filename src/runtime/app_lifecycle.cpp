#include "app.h"
#include "log/log.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>

namespace meowody {
namespace {
godot::SceneTree* scene_tree() {
    return godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
}
}
bool App::lifecycle_available() const {
    return singleton == this && !stopping && lifecycle_attached && main_thread_center &&
        main_thread_center->is_main_thread() && !godot::Engine::get_singleton()->is_editor_hint();
}
void App::set_game_paused(bool paused) {
    if (!lifecycle_available() || lifecycle.quitting) return;
    lifecycle.game_paused = paused;
    if (auto* tree = scene_tree()) tree->set_pause(paused || lifecycle.background);
}
SaveStatus App::flush_lifecycle_save(ApplicationSaveReason reason) {
    auto status = save_center->flush_pending();
    if (!status) event_center->emit(ApplicationSaveFailed{reason, status});
    return status;
}
void App::update_background() {
    if (!lifecycle_available() || lifecycle_busy || lifecycle.quitting) return;
    lifecycle_busy = true;
    struct Guard { bool& value; ~Guard() { value = false; } } guard{lifecycle_busy};
    while (!lifecycle.quitting && lifecycle.background != (lifecycle.system_paused || lifecycle.minimized)) {
        auto* tree = scene_tree();
        bool background = lifecycle.system_paused || lifecycle.minimized;
        if (background && tree) lifecycle.game_paused = tree->is_paused();
        lifecycle.background = background;
        late_pending = false;
        if (background) background_started = godot::Time::get_singleton()->get_ticks_usec();
        if (tree) tree->set_pause(background || lifecycle.game_paused);
        main_thread_center->set_suspended(background);
        coroutine_center->set_suspended(background);
        audio_center->set_suspended(background);
        tween_center->set_suspended(background);
        download_center->set_suspended(background);
        resource_center->set_suspended(background);
        net_center->set_suspended(background);
        ui_center->set_suspended(background);
        input_center->set_focused(lifecycle.focused && !background);
        if (background) {
            event_center->emit(ApplicationEvent{ApplicationEventKind::Suspended, lifecycle});
            flush_lifecycle_save(ApplicationSaveReason::Suspend);
        } else {
            resume_frame = true;
            double seconds = double(godot::Time::get_singleton()->get_ticks_usec() - background_started) / 1000000.0;
            net_center->check_now();
            event_center->emit(ApplicationEvent{ApplicationEventKind::Resumed, lifecycle, seconds});
        }
    }
}
void App::low_memory() {
    if (!lifecycle_available() || memory_busy || lifecycle.quitting) return;
    memory_busy = true;
    struct Guard { bool& value; ~Guard() { value = false; } } guard{memory_busy};
    event_center->emit(ApplicationEvent{ApplicationEventKind::LowMemory, lifecycle});
    flush_lifecycle_save(ApplicationSaveReason::LowMemory);
    resource_center->clear_lru();
    ui_center->clear_cache();
    for (const auto& name : pool_center->names()) pool_center->clear_idle(name);
}
void App::request_back() {
    if (!lifecycle_available() || request_busy || quit_busy || lifecycle.quitting) return;
    request_busy = true;
    bool handled = ui_center->back();
    if (!handled) {
        ApplicationRequest request(ApplicationRequestKind::Back);
        event_center->emit(request);
        handled = request.is_consumed();
    }
    request_busy = false;
    if (!handled && !lifecycle.quitting) request_quit();
}
void App::request_quit() {
    if (!lifecycle_available() || request_busy || quit_busy || lifecycle.quitting) return;
    request_busy = true;
    ApplicationRequest request(ApplicationRequestKind::Quit);
    event_center->emit(request);
    request_busy = false;
    if (!request.is_consumed() && !lifecycle.quitting) quit();
}
bool App::quit(bool force) {
    if (!lifecycle_available() || quit_busy) return false;
    if (lifecycle.quitting) return true;
    quit_busy = true;
    struct Guard { bool& value; ~Guard() { value = false; } } guard{quit_busy};
    event_center->emit(ApplicationEvent{ApplicationEventKind::Quitting, lifecycle});
    auto saved = flush_lifecycle_save(ApplicationSaveReason::Quit);
    if (!saved && !force) return false;
    auto* tree = scene_tree();
    if (!tree) return false;
    if (!saved) Log::warn(godot::StringName("App")) << "Forced quit with unsaved data: " << saved.message;
    lifecycle.quitting = true;
    late_pending = false;
    main_thread_center->set_suspended(true);
    coroutine_center->set_suspended(true);
    download_center->set_suspended(true);
    resource_center->set_suspended(true);
    net_center->set_suspended(true);
    tree->quit();
    return true;
}
}
