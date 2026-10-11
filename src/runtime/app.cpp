#include "app.h"
#include "godot_cpp/variant/utility_functions.hpp"

#include "godot_cpp/classes/engine.hpp"
#include "log/log.hpp"
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

namespace meowody {
App *App::singleton = nullptr;

Proxy<CoroutineCenter> App::coroutine{
	&App::get_coroutine_center
};
Proxy<EventCenter> App::event{
	&App::get_event_center
};
Proxy<TweenCenter> App::tween{
	&App::get_tween_center
};
Proxy<DownloadCenter> App::download{
	&App::get_download_center
};
Proxy<ResourceCenter> App::resource{
	&App::get_resource_center
};
Proxy<SaveCenter> App::save{
	&App::get_save_center
};
Proxy<PoolCenter> App::pool{
	&App::get_pool_center
};
Proxy<TimeCenter> App::time{
	&App::get_time_center
};
Proxy<AudioCenter> App::audio{
	&App::get_audio_center
};
Proxy<NetCenter> App::net{
	&App::get_net_center
};
Proxy<SceneCenter> App::scene{
	&App::get_scene_center
};
Proxy<DebugCenter> App::debug{
	&App::get_debug_center
};
Proxy<UICenter> App::ui{
	&App::get_ui_center
};

Proxy<SystemCenter> App::system{ &App::get_system_center };

Proxy<MainThreadCenter> App::main_thread{ &App::get_main_thread_center };
Proxy<TaskCenter> App::task{ &App::get_task_center };
Proxy<InputCenter> App::input{ &App::get_input_center };

void App::initialize() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = memnew(App);
	godot::Engine::get_singleton()->register_singleton("App", singleton);
	singleton->startup();
	// Scene initialization precedes creation of the main loop/root window.
	singleton->call_deferred("attach_to_tree");
}

App::~App() {
	if (singleton != this)
		return;
	shutdown();
	godot::Engine::get_singleton()->unregister_singleton("App");
	singleton = nullptr;
}

void App::attach_to_tree() {
	if (singleton != this || stopping || is_inside_tree() || godot::Engine::get_singleton()->is_editor_hint())
		return;
	auto *tree = godot::Object::cast_to<godot::SceneTree>(godot::Engine::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root())
		return;
	set_name("App");
	set_process_mode(Node::PROCESS_MODE_ALWAYS);
	tree->get_root()->add_child(this);
	set_process(true);
}
void App::finalize() {
	if (singleton == nullptr)
		return;
	ERR_FAIL_COND_MSG(singleton->updating, "Finalize Runtime after update has returned");
	memdelete(singleton);
}
App *App::get_singleton() { return singleton; }

CoroutineCenter &App::get_coroutine_center() {
	DEV_ASSERT(singleton != nullptr);
	DEV_ASSERT(singleton->coroutine_center != nullptr);
	return *singleton->coroutine_center;
}

EventCenter &App::get_event_center() {
	DEV_ASSERT(singleton != nullptr);
	DEV_ASSERT(singleton->event_center != nullptr);
	return *singleton->event_center;
}

TweenCenter &App::get_tween_center() {
	DEV_ASSERT(singleton && singleton->tween_center);
	return *singleton->tween_center;
}

DownloadCenter &App::get_download_center() {
	DEV_ASSERT(singleton && singleton->download_center);
	return *singleton->download_center;
}

ResourceCenter &App::get_resource_center() {
	DEV_ASSERT(singleton && singleton->resource_center);
	return *singleton->resource_center;
}

SaveCenter &App::get_save_center() {
	DEV_ASSERT(singleton && singleton->save_center);
	return *singleton->save_center;
}

PoolCenter &App::get_pool_center() {
	DEV_ASSERT(singleton && singleton->pool_center);
	return *singleton->pool_center;
}
TimeCenter &App::get_time_center() {
	DEV_ASSERT(singleton && singleton->time_center);
	return *singleton->time_center;
}
AudioCenter &App::get_audio_center() {
	DEV_ASSERT(singleton && singleton->audio_center);
	return *singleton->audio_center;
}
NetCenter &App::get_net_center() {
	DEV_ASSERT(singleton && singleton->net_center);
	return *singleton->net_center;
}
SceneCenter &App::get_scene_center() {
	DEV_ASSERT(singleton && singleton->scene_center);
	return *singleton->scene_center;
}
DebugCenter &App::get_debug_center() {
	DEV_ASSERT(singleton && singleton->debug_center);
	return *singleton->debug_center;
}
UICenter &App::get_ui_center() {
	DEV_ASSERT(singleton && singleton->ui_center);
	return *singleton->ui_center;
}

SystemCenter &App::get_system_center() {
	DEV_ASSERT(singleton && singleton->system_center);
	return *singleton->system_center;
}

MainThreadCenter &App::get_main_thread_center() {
	DEV_ASSERT(singleton && singleton->main_thread_center); return *singleton->main_thread_center;
}
TaskCenter &App::get_task_center() {
	DEV_ASSERT(singleton && singleton->task_center); return *singleton->task_center;
}
InputCenter &App::get_input_center() {
	DEV_ASSERT(singleton && singleton->input_center); return *singleton->input_center;
}
void App::_notification(int what) {
	if (!input_center) return;
	if (what == NOTIFICATION_APPLICATION_FOCUS_OUT) input_center->set_focused(false);
	else if (what == NOTIFICATION_APPLICATION_FOCUS_IN) input_center->set_focused(true);
}

void App::_process(double p_delta_time) {
	if (singleton != this || stopping || updating || late_pending)
		return;
	singleton->updating = true;
	struct Guard {
		bool &flag;
		~Guard() { flag = false; }
	} guard{ singleton->updating };
	singleton->main_thread_center->update();
	singleton->time_center->update();
	singleton->download_center->update();
	singleton->resource_center->update();
	singleton->net_center->update();
	singleton->audio_center->update(p_delta_time);
	singleton->scene_center->update(p_delta_time);
	singleton->ui_center->update(p_delta_time);
	debug_center->update(p_delta_time);
	late_delta = p_delta_time;
	late_pending = true;
	call_deferred("late_process");
}

void App::late_process() {
	if (singleton != this || stopping || updating || !late_pending)
		return;
	late_pending = false;
	updating = true;
	struct Guard {
		bool &flag;
		~Guard() { flag = false; }
	} guard{ updating };
	tween_center->update();
	coroutine_center->update(late_delta);
	save_center->flush_frame(godot::Engine::get_singleton()->get_process_frames());
}

void App::startup() {
	Log::set_default_tag("meowody");
	Log::set_level(Log::Level::Debug);
	Log::set_rich_enabled(true);
	godot::UtilityFunctions::print("[Runtime] startup");
	main_thread_center = new MainThreadCenter;
	task_center = new TaskCenter(*main_thread_center);
	input_center = new InputCenter;
	event_center = new EventCenter;
	coroutine_center = new CoroutineCenter;
	download_center = new DownloadCenter;
	resource_center = new ResourceCenter(*download_center);
	save_center = new SaveCenter;
	pool_center = new PoolCenter;
	time_center = new TimeCenter;
	audio_center = new AudioCenter;
	net_center = new NetCenter;
	scene_center = new SceneCenter(*resource_center);
	ui_center = new UICenter(*resource_center, scene_center);
	tween_center = new TweenCenter;
	debug_center = new DebugCenter;
	system_center = new SystemCenter;
	time_center->sync_network(godot::String("time.cloudflare.com"));
	// Runtime flushes after tween/coroutine callbacks in late_process.
	// Do not enqueue a separate save callback before that phase.
}

void App::shutdown() {
	if (stopping)
		return;
	godot::UtilityFunctions::print("[Runtime] shutdown");
	stopping = true;
	set_process(false);
	late_pending = false;
	ui_center->shutdown();
	delete ui_center;
	ui_center = nullptr;
	system_center->shutdown();
	// Join workers while registered business instances and lower services still exist.
	task_center->shutdown();
	main_thread_center->shutdown();
	delete system_center;
	system_center = nullptr;
	delete task_center;
	task_center = nullptr;
	delete main_thread_center;
	main_thread_center = nullptr;
	input_center->shutdown();
	delete input_center;
	input_center = nullptr;
	debug_center->shutdown();
	delete debug_center;
	debug_center = nullptr;
	tween_center->shutdown();
	delete tween_center;
	tween_center = nullptr;
	coroutine_center->clear();
	time_center->shutdown();
	scene_center->shutdown();
	audio_center->shutdown();
	net_center->shutdown();
	pool_center->shutdown();
	save_center->shutdown();
	resource_center->shutdown();
	download_center->shutdown();
	event_center->clear_all();
	delete scene_center;
	scene_center = nullptr;
	delete audio_center;
	audio_center = nullptr;
	delete net_center;
	net_center = nullptr;
	delete time_center;
	time_center = nullptr;
	delete pool_center;
	pool_center = nullptr;
	delete save_center;
	save_center = nullptr;
	delete resource_center;
	resource_center = nullptr;
	delete download_center;
	download_center = nullptr;
	delete event_center;
	event_center = nullptr;
	delete coroutine_center;
	coroutine_center = nullptr;
	Log::shutdown();
}

}; //namespace meowody
