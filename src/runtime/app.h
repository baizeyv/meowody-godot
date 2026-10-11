#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/node.hpp>

#include "audio/audio_center.h"
#include "coroutine/coroutine_center.h"
#include "debug/debug_center.h"
#include "download/download_center.h"
#include "event/event_center.h"
#include "net/net_center.h"
#include "main_thread/main_thread_center.h"
#include "task/task_center.h"
#include "input/input_center.h"
#include "pool/pool_center.h"
#include "proxy.hpp"
#include "resource/resource_center.h"
#include "save/save_center.h"
#include "scene/scene_center.h"
#include "system/system_center.h"
#include "time/time_center.h"
#include "tween/tween_center.h"
#include "ui/ui_center.h"

namespace meowody {
class App : public godot::Node {
	GDREFLECT_CLASS(App, godot::Node)

private:
	static App *singleton;

	EventCenter *event_center = nullptr;
	CoroutineCenter *coroutine_center = nullptr;
	TweenCenter *tween_center = nullptr;
	DownloadCenter *download_center = nullptr;
	ResourceCenter *resource_center = nullptr;
	SaveCenter *save_center = nullptr;
	PoolCenter *pool_center = nullptr;
	TimeCenter *time_center = nullptr;
	AudioCenter *audio_center = nullptr;
	NetCenter *net_center = nullptr;
	SceneCenter *scene_center = nullptr;
	DebugCenter *debug_center = nullptr;
	UICenter *ui_center = nullptr;
	SystemCenter *system_center = nullptr;
	MainThreadCenter *main_thread_center = nullptr;
	TaskCenter *task_center = nullptr;
	InputCenter *input_center = nullptr;

	static CoroutineCenter &get_coroutine_center();
	static EventCenter &get_event_center();
	static TweenCenter &get_tween_center();
	static DownloadCenter &get_download_center();
	static ResourceCenter &get_resource_center();
	static SaveCenter &get_save_center();
	static PoolCenter &get_pool_center();
	static TimeCenter &get_time_center();
	static AudioCenter &get_audio_center();
	static NetCenter &get_net_center();
	static SceneCenter &get_scene_center();
	static DebugCenter &get_debug_center();
	static UICenter &get_ui_center();
	static SystemCenter &get_system_center();
	static MainThreadCenter &get_main_thread_center();
	static TaskCenter &get_task_center();
	static InputCenter &get_input_center();

public:
	App() = default;
	~App() override;

	static App *get_singleton();

	static void initialize();
	static void finalize();

	static Proxy<CoroutineCenter> coroutine;
	static Proxy<EventCenter> event;
	static Proxy<TweenCenter> tween;
	static Proxy<DownloadCenter> download;
	static Proxy<ResourceCenter> resource;
	static Proxy<SaveCenter> save;
	static Proxy<PoolCenter> pool;
	static Proxy<TimeCenter> time;
	static Proxy<AudioCenter> audio;
	static Proxy<NetCenter> net;
	static Proxy<SceneCenter> scene;
	static Proxy<DebugCenter> debug;
	static Proxy<UICenter> ui;
	static Proxy<SystemCenter> system;
	static Proxy<MainThreadCenter> main_thread;
	static Proxy<TaskCenter> task;
	static Proxy<InputCenter> input;

	void _process(double delta) override;
	void _notification(int what);

	GDMETHOD
	void late_process();
	GDMETHOD
	void attach_to_tree();

private:
	bool updating = false;
	bool stopping = false;
	bool late_pending = false;
	double late_delta = 0.0;

	void startup();
	void shutdown();
};
}; //namespace meowody
