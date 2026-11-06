#include "runtime/app.h"
#include "register_types.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <thread>

using namespace godot;
using namespace meowody;
void initialize_module(ModuleInitializationLevel);
void uninitialize_module(ModuleInitializationLevel);
class LifecycleProbe : public RefCounted {
    GDCLASS(LifecycleProbe, RefCounted)
    std::vector<Connection> connections;
    AudioHandle ordinary, manual, background_voice;
    tween::TweenHandle animation, manual_tween, background_tween, manual_clock;
    UIHandle ui_handle;
    uint64_t ui_root = 0, pool_node = 0;
    CoroutineCenter::CoroutineId coroutine = 0;
    int coroutine_ticks = 0, completion_count = 0, suspended = 0, resumed = 0, focus_events = 0;
    int back_requests = 0, quit_requests = 0, save_errors = 0, quitting_events = 0;
    double animation_before = 0, ui_before = 0, resume_seconds = 0;
    bool consume_back = true, consume_quit = true;
    TaskHandle task;
    LoadHandle<> cached_resource;
    BatchLoadHandle pinned;
    DownloadHandle manual_download, active_download, new_download, cancelled_download;
    std::vector<DownloadHandle> simple_downloads;
    String url;
    String failure_directory;
    int failure_sequence = 0;
    ApplicationSaveReason last_save_reason = ApplicationSaveReason::Quit;
    bool ok = true;
    App* app() const { return App::get_singleton(); }
    SceneTree* tree() const { return Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop()); }
    void check(bool value, const char* message) {
        UtilityFunctions::print("[LifecycleTest] ", value ? "PASS " : "FAIL ", message);
        ok &= value;
    }
    Ref<AudioStreamWAV> sound() {
        Ref<AudioStreamWAV> stream; stream.instantiate();
        stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
        stream->set_mix_rate(22050);
        PackedByteArray samples; samples.resize(22050 * 2);
        stream->set_data(samples);
        stream->set_loop_mode(AudioStreamWAV::LOOP_FORWARD);
        stream->set_loop_end(22050);
        return stream;
    }
    AnimationPlayer* ui_animation() const {
        auto* root = Object::cast_to<Node>(ObjectDB::get_instance(ui_root));
        return root ? Object::cast_to<AnimationPlayer>(root->get_node_or_null("AnimationPlayer")) : nullptr;
    }
    void subscribe() {
        if (!connections.empty()) return;
        App::time->cancel_sync();
        connections.push_back(App::event->connect<ApplicationEvent>([this](const ApplicationEvent& event) {
            if (event.kind == ApplicationEventKind::Suspended) {
                ++suspended;
                Dictionary data; data["value"] = 42;
                check(App::save->save("lifecycle", "slot", data).ok(), "business snapshot accepted before suspend flush");
                app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED); // Duplicate/reentrant notification.
            } else if (event.kind == ApplicationEventKind::Resumed) { ++resumed; resume_seconds = event.background_seconds; }
            else if (event.kind == ApplicationEventKind::FocusChanged) ++focus_events;
            else if (event.kind == ApplicationEventKind::Quitting) {
                ++quitting_events;
                app()->request_quit(); // Must not recurse into another finalization.
            }
        }));
        connections.push_back(App::event->connect<ApplicationRequest>([this](const ApplicationRequest& request) {
            if (request.kind == ApplicationRequestKind::Back) { ++back_requests; if (consume_back) request.consume(); }
            else { ++quit_requests; if (consume_quit) request.consume(); app()->request_quit(); }
        }));
        connections.push_back(App::event->connect<ApplicationSaveFailed>([this](const ApplicationSaveFailed& event) {
            ++save_errors;
            last_save_reason = event.reason;
            check(!event.status.ok(), "save failure event carries actual error");
            if (event.reason == ApplicationSaveReason::Quit) check(!app()->quit(), "reentrant quit rejected during save failure");
        }));
    }
    void fail_save() {
        failure_directory = "user://failed_save_" + OS::get_singleton()->get_environment("LIFECYCLE_CASE") + "_" + String::num_int64(failure_sequence++);
        check(App::save->configure(failure_directory).ok(), "configure failure-test save directory");
        DirAccess::make_dir_recursive_absolute(failure_directory);
        Dictionary data; data["value"] = 77;
        check(App::save->save("data", "slot", data).ok(), "stage data before simulated disk failure");
        DirAccess::remove_absolute(failure_directory);
        auto blocker = FileAccess::open(failure_directory, FileAccess::WRITE);
        if (blocker.is_valid()) { blocker->store_string("not a directory"); blocker.unref(); }
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("phase", "number"), &LifecycleProbe::phase);
        ClassDB::bind_method(D_METHOD("snapshot"), &LifecycleProbe::snapshot);
        ClassDB::bind_method(D_METHOD("begin_network", "url"), &LifecycleProbe::begin_network);
        ClassDB::bind_method(D_METHOD("network_received"), &LifecycleProbe::network_received);
        ClassDB::bind_method(D_METHOD("pause_network"), &LifecycleProbe::pause_network);
        ClassDB::bind_method(D_METHOD("resume_network"), &LifecycleProbe::resume_network);
        ClassDB::bind_method(D_METHOD("network_done"), &LifecycleProbe::network_done);
        ClassDB::bind_method(D_METHOD("check_network"), &LifecycleProbe::check_network);
        ClassDB::bind_method(D_METHOD("exit_case", "force"), &LifecycleProbe::exit_case);
        ClassDB::bind_method(D_METHOD("stop_test_audio"), &LifecycleProbe::stop_test_audio);
    }
public:
    ~LifecycleProbe() {
        connections.clear();
        if (App::get_singleton()) {
            App::coroutine->stop(coroutine);
            animation.kill(); manual_tween.kill(); background_tween.kill(); manual_clock.kill();
            ordinary.stop(); manual.stop(); background_voice.stop();
            if (App::pool->contains("lifecycle_pool")) App::pool->remove("lifecycle_pool", true);
        }
    }
    Dictionary snapshot() const {
        auto state = app()->application_state(); Dictionary result;
        result["background"] = state.background; result["system_paused"] = state.system_paused;
        result["minimized"] = state.minimized; result["quitting"] = state.quitting;
        result["quit_requests"] = quit_requests; result["suspended"] = suspended; result["resumed"] = resumed;
        return result;
    }
    bool phase(int number) {
        if (number == 0) {
            subscribe();
            check(app()->is_inside_tree() && !tree()->is_auto_accept_quit() && !tree()->is_quit_on_go_back(), "App owns runtime quit routing");
            App::save->configure("user://lifecycle_save");
            {
                NetCenter net([] { return true; });
                int delivered = 0;
                net.get("invalid", [&](const NetResult&) { ++delivered; net.set_suspended(true); });
                net.get("invalid2", [&](const NetResult&) { ++delivered; });
                net.update();
                check(delivered == 1, "HTTP completion suspension preserves remaining callbacks");
                net.set_suspended(false); net.update();
                check(delivered == 2, "deferred HTTP completion delivered once after resume");
            }
            {
                ResourceCenter resources;
                ResourceOptions options; options.mode = ResourceMode::Source;
                resources.initialize(options);
                int delivered = 0;
                auto first = resources.load_async<>("unknown", [&](const LoadResult<>&) { ++delivered; resources.set_suspended(true); });
                auto second = resources.load_async<>("unknown", [&](const LoadResult<>&) { ++delivered; });
                resources.update();
                check(delivered == 1, "resource completion suspension preserves remaining callbacks");
                resources.set_suspended(false); resources.update();
                check(delivered == 2, "deferred resource completion delivered once after resume");
            }
            app()->_notification(Node::NOTIFICATION_APPLICATION_FOCUS_IN);
            auto count = focus_events;
            app()->_notification(Node::NOTIFICATION_APPLICATION_FOCUS_OUT);
            app()->_notification(Node::NOTIFICATION_APPLICATION_FOCUS_OUT);
            check(focus_events == count + 1 && !tree()->is_paused(), "ordinary focus loss deduplicates without pausing");
            app()->_notification(Node::NOTIFICATION_APPLICATION_FOCUS_IN);
            ordinary = App::audio->play("SFX", sound());
            manual = App::audio->play("SFX", sound()); manual.pause(); App::audio->update(0);
            animation = tween::interval(60);
            manual_clock = tween::interval(60); manual_clock.process_mode(tween::UpdateMode::Manual);
            manual_tween = tween::interval(60); manual_tween.pause();
            auto flow = Coroutine::make();
            flow->push_step(Coroutine::Step([this](double) { ++coroutine_ticks; return false; }));
            coroutine = App::coroutine->start(flow);
            UIOptions panel_options; panel_options.kind = UIKind::Modal; panel_options.cache = false;
            App::ui->configure("panel", panel_options);
            ui_handle = App::ui->open("panel");
            check(ui_handle.root() != nullptr, "lifecycle UI scene loaded by address");
            if (!ui_handle.root()) return false;
            ui_root = uint64_t(ui_handle.root()->get_instance_id());
            auto cached = App::ui->open("cached"); cached.close();
            App::pool->create<Node>("lifecycle_pool", {2, 0, 2});
            pool_node = App::pool->acquire("lifecycle_pool", tree()->get_root())->get_instance_id();
            App::resource->set_lru_capacity(4);
            cached_resource = App::resource->load_async<>("spare");
            pinned = App::resource->preload_group("pinned", std::vector<String>{"res"});
        } else if (number == 1) {
            animation_before = animation.position();
            manual_clock.custom_step(0.5);
            if (auto* player = ui_animation()) ui_before = player->get_current_animation_position();
            manual_download = App::download->download("http://127.0.0.1:1/manual"); manual_download.pause();
            auto other = App::download->download("http://127.0.0.1:1/resumed_in_background"); other.pause(); simple_downloads.push_back(other);
            App::main_thread->post([this] { app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED); });
            App::main_thread->post([this] { ++completion_count; });
            App::main_thread->update();
            check(completion_count == 0 && suspended == 1 && tree()->is_paused(), "suspend stops remaining task completions and deduplicates notifications");
            auto disk = App::save->inspect("lifecycle", true);
            check(disk.ok() && Dictionary(disk.data.get("slot", Dictionary())).get("value", 0) == Variant(42), "suspend notification snapshots are committed synchronously");
            check(ordinary.status() == VoiceStatus::Paused && manual.status() == VoiceStatus::Paused && !App::audio->channel_options("SFX").paused, "audio lifecycle pause preserves channel settings");
            background_voice = App::audio->play("SFX", sound());
            background_tween = tween::interval(60); background_tween.play();
            check(background_voice.status() == VoiceStatus::Paused && background_tween.position() == 0, "new background audio and tween remain suspended");
            check(manual_download.paused(), "download manually paused before suspend stays paused");
            other.resume();
            check(other.paused(), "manual resume cannot bypass lifecycle suspension");
            auto cancelled = App::download->download("http://127.0.0.1:1/cancel"); cancelled.cancel(); simple_downloads.push_back(cancelled);
            task = App::task->submit([](const TaskContext&) { return TaskResult{}; }, [this](const TaskResult&) { ++completion_count; });
        } else if (number == 2) {
            auto before_ticks = coroutine_ticks;
            app()->_process(100); app()->late_process(); App::coroutine->update(100);
            manual_clock.custom_step(100);
            check(manual_clock.position() == 0.5, "manual tween stepping respects lifecycle suspension");
            check(coroutine_ticks == before_ticks && completion_count == 0, "background blocks coroutine and main-thread delivery");
            check(animation.position() == animation_before && background_tween.position() == 0, "automatic tweens do not advance in background");
            if (auto* player = ui_animation()) check(player->get_current_animation_position() == ui_before, "UI animation is frozen in background");
            check(task.status() == TaskStatus::AwaitingMainThread, "worker may finish while its completion remains queued");
            app()->set_game_paused(true);
            app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED);
            app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED);
            check(resumed == 1 && resume_seconds > 0 && tree()->is_paused(), "resume reports duration and preserves manual game pause");
            check(ordinary.status() == VoiceStatus::Playing && manual.status() == VoiceStatus::Paused, "resume restores only lifecycle-paused voices");
            check(manual_tween.status() == tween::Status::Paused && animation.status() == tween::Status::Playing, "resume preserves manually paused tweens");
            check(manual_download.paused() && simple_downloads[0].status() == DownloadStatus::Queued && simple_downloads[1].status() == DownloadStatus::Cancelled, "resume preserves manual download and cancellation states");
            App::download->cancel_all();
            app()->set_game_paused(false);
            manual_clock.custom_step(0.25);
            check(manual_clock.position() == 0.75, "manual tween stepping continues after resume");
            app()->set_game_paused(true);
            app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED);
            app()->set_game_paused(false);
            check(tree()->is_paused(), "clearing manual pause cannot bypass background pause");
            app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED);
            check(!tree()->is_paused(), "manual pause changes made in background apply on resume");
            app()->_process(100); app()->late_process();
            check(coroutine_ticks == before_ticks && completion_count == 2, "resume frame drains completions without advancing game time");
            app()->_process(0.01); app()->late_process();
            check(coroutine_ticks == before_ticks + 1, "game coroutine continues on the following frame");
            tree()->set_pause(true);
            app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED);
            app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED);
            check(tree()->is_paused(), "pre-existing SceneTree pause survives lifecycle transition");
            app()->set_game_paused(false);
        } else if (number == 3) {
            check(cached_resource.succeeded() && pinned.succeeded(), "cache and pinned resources prepared");
            check(App::ui->cache_size() == 1, "inactive UI available for memory cleanup");
            app()->_notification(Node::NOTIFICATION_OS_MEMORY_WARNING);
            auto pool = App::pool->stats("lifecycle_pool");
            check(pool.exists && pool.idle == 0 && pool.active == 1 && ObjectDB::get_instance(pool_node), "low memory clears only idle pooled nodes");
            check(App::ui->cache_size() == 0 && ui_handle.valid() && ObjectDB::get_instance(ui_root), "low memory preserves active UI");
            check(App::resource->lru_size() == 0 && App::resource->has_group("pinned") && App::resource->has_cached("res"), "low memory preserves pinned resources");
            app()->request_back();
            check(back_requests == 0, "back request closes UI before notifying business");
            app()->request_back();
            check(back_requests == 1 && !app()->application_state().quitting, "business may consume back request");
            app()->request_quit();
            check(quit_requests == 1 && !app()->application_state().quitting, "quit may be consumed without recursive requests");
            fail_save();
            check(!app()->quit() && save_errors == 1 && App::save->pending_count() == 1 && !app()->application_state().quitting, "failed save prevents quit and retains pending data");
            DirAccess::remove_absolute(failure_directory); DirAccess::make_dir_recursive_absolute(failure_directory);
            check(App::save->flush_pending().ok() && App::save->pending_count() == 0, "failed save can be retried without losing data");
        } else if (number == 4) {
            fail_save();
            auto failures = save_errors;
            app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED);
            check(app()->application_state().background && tree()->is_paused() && save_errors == failures + 1 && last_save_reason == ApplicationSaveReason::Suspend, "failed suspend save reports error without blocking suspension");
            auto pending = App::save->pending_count();
            app()->_notification(Node::NOTIFICATION_OS_MEMORY_WARNING);
            check(last_save_reason == ApplicationSaveReason::LowMemory && App::save->pending_count() == pending, "low-memory save failure retains pending snapshots");
            app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED);
            DirAccess::remove_absolute(failure_directory); DirAccess::make_dir_recursive_absolute(failure_directory);
            check(App::save->flush_pending().ok() && int(App::save->load("data", "slot").data.get("value", 0)) == 77, "background save failure recovers original data after retry");
        }
        return ok;
    }
    void begin_network(const String& source) {
        subscribe(); url = source;
        DownloadOptions options; options.max_bytes = 4 * 1024 * 1024;
        active_download = App::download->download(url + "/active", options, [this](const DownloadResult& result) { check(bool(result), "resumed download succeeds"); ++completion_count; });
        manual_download = App::download->download(url + "/manual", options); manual_download.pause();
    }
    int64_t network_received() const { return active_download.downloaded_bytes(); }
    bool pause_network() {
        app()->_notification(Node::NOTIFICATION_APPLICATION_PAUSED);
        new_download = App::download->download(url + "/new");
        cancelled_download = App::download->download(url + "/cancel"); cancelled_download.cancel();
        check(active_download.paused() && new_download.paused() && completion_count == 0, "active and newly queued HTTP downloads pause immediately");
        return ok;
    }
    void resume_network() { app()->_notification(Node::NOTIFICATION_APPLICATION_RESUMED); }
    bool network_done() const { return active_download.finished() && new_download.finished(); }
    bool check_network() {
        check(active_download.succeeded() && new_download.succeeded() && completion_count == 1, "HTTP downloads resume with one completion");
        check(manual_download.paused() && cancelled_download.status() == DownloadStatus::Cancelled, "HTTP resume does not revive manual pause or cancellation");
        check(FileAccess::get_sha256(active_download.result().path) == OS::get_singleton()->get_environment("LIFECYCLE_SHA"), "resumed HTTP bytes retain integrity");
        App::download->cancel_all(); return ok;
    }
    bool exit_case(bool force) {
        subscribe();
        if (force) fail_save();
        else {
            auto previous_requests = quit_requests;
            app()->request_quit();
            check(quit_requests == previous_requests + 1 && !app()->application_state().quitting, "confirmation request is intercepted");
            Dictionary data; data["confirmed"] = true; App::save->save("exit", "slot", data);
        }
        check(app()->quit(force), "explicit confirmation or force accepts final quit");
        check(app()->application_state().quitting && quitting_events == 1, "final quit dispatched exactly once");
        if (force) check(save_errors == 1 && App::save->pending_count() == 1, "force quit explicitly accepts failed save");
        else check(App::save->inspect("exit", true).ok(), "normal exit commits staged save");
        return ok;
    }
    void stop_test_audio() { ordinary.stop(); manual.stop(); background_voice.stop(); App::audio->update(0); }
};
static void initialize_probe(ModuleInitializationLevel level) {
    initialize_module(level);
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<LifecycleProbe>();
}
extern "C" GDExtensionBool GDE_EXPORT lifecycle_test_init(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init(proc, library, initialization);
    init.register_initializer(initialize_probe); init.register_terminator(uninitialize_module);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
