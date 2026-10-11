#pragma once
#include "ui_presenter.h"
#include "runtime/resource/resource_center.h"
#include <godot_cpp/classes/packed_scene.hpp>
#include <map>
#include <list>
namespace meowody {
class SceneCenter;
class UICenter {
    struct Config { UIOptions options; UIPresenterFactory factory; };
    struct Instance {
        godot::String path;
        std::shared_ptr<Config> config;
        std::shared_ptr<detail::UIState> state;
        godot::Variant data;
        std::shared_ptr<UIPresenter> presenter;
        LoadHandle<godot::PackedScene> load;
        std::vector<UIOpenCallback> callbacks;
        uint64_t layer = 0, order = 0, focus = 0, animation = 0;
        godot::StringName playing;
        uint64_t transition=0;
        godot::StringName enter_name, exit_name;
        double elapsed = 0;
        bool asynchronous = false, data_pending = false, attached = false;
    };
    struct Cached { godot::String path; uint64_t root; };
    ResourceCenter& resources;
    std::map<godot::String,std::shared_ptr<Config>> configs;
    std::vector<std::shared_ptr<Instance>> instances, retired;
    std::list<Cached> cache;
    Connection scene_connection;
    uint64_t next_id=1, next_order=1, restore_focus=0;
    size_t capacity=16;
    bool stopped=false, busy=false;
    godot::String normalize(const godot::String&) const;
    UIHandle request(const godot::String&, const godot::Variant&, bool, UIOpenCallback);
    void start(Instance&);
    bool mount(Instance&);
    void deliver_data(Instance&);
    void resolve_transition(Instance&);
    bool animate(Instance&, bool entering);
    void complete_open(Instance&);
    void begin_close(Instance&);
    void retire(const std::shared_ptr<Instance>&, bool cache_allowed=true);
    void refresh_layers();
    void evict();
    void drain_shutdown();
public:
    explicit UICenter(ResourceCenter&, SceneCenter* scenes=nullptr);
    ~UICenter();
    UICenter(const UICenter&)=delete;
    UICenter& operator=(const UICenter&)=delete;
    bool configure(const godot::String& scene, UIOptions options, UIPresenterFactory factory={});
    UIHandle open(const godot::String& scene, const godot::Variant& data={});
    UIHandle open_async(const godot::String& scene, const godot::Variant& data={}, UIOpenCallback callback={});
    UIHandle get(uint64_t id) const;
    UIHandle find(const godot::String& scene) const;
    bool close(uint64_t id);
    bool close(const godot::String& scene);
    void close_all(bool include_persistent=true);
    bool back();
    bool is_open(const godot::String& scene) const { return find(scene).is_open(); }
    float progress_percent(const godot::String& scene) const { return find(scene).progress_percent(); }
    size_t active_count() const { return instances.size(); }
    size_t cache_size() const { return cache.size(); }
    void set_cache_capacity(size_t value) { capacity=value; evict(); }
    void clear_cache();
    void update(double delta=0.0);
    void shutdown();
    bool is_closed() const { return stopped; }
};
}
