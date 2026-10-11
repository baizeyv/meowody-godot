#pragma once
#include "scene_types.h"
#include "runtime/event/event_center.hpp"
#include "runtime/resource/resource_center.h"
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node.hpp>
namespace meowody {
class SceneCenter {
public:
    explicit SceneCenter(ResourceCenter& resources);
    ~SceneCenter();
    SceneCenter(const SceneCenter&) = delete;
    SceneCenter& operator=(const SceneCenter&) = delete;
    SceneHandle switch_to(const godot::String& path, SceneOptions options = {}, Delegate<void(const SceneResult&)> callback = {});
    SceneHandle switch_pack(const godot::String& url, const godot::String& entry, SceneOptions options = {}, Delegate<void(const SceneResult&)> callback = {});
    SceneHandle reload(SceneOptions options = {}, Delegate<void(const SceneResult&)> callback = {});
    godot::Node* current_scene() const;
    bool is_switching() const { return m_job && !m_job->handle.is_finished(); }
    SceneHandle current_transition() const;
    template<class F> Connection on_switched(F&& callback) { return notifications.connect<SceneResult>(std::forward<F>(callback)); }
    void update(double delta);
    void shutdown();
    bool is_closed() const { return m_closed; }
private:
    struct Job { SceneHandle handle; LoadHandle<godot::PackedScene> load; SceneOptions options; Delegate<void(const SceneResult&)> callback; double elapsed = 0; uint64_t next_scene = 0; };
    EventCenter notifications;
    ResourceCenter& m_resources;
    std::shared_ptr<Job> m_job;
    std::vector<std::shared_ptr<Job>> m_delivery;
    uint64_t m_layer = 0, m_rect = 0;
    bool m_closed = false, m_updating = false;
    SceneHandle begin(LoadHandle<godot::PackedScene> load, SceneOptions options, Delegate<void(const SceneResult&)> callback);
    bool overlay();
    void opacity(float alpha);
    void finish(Job& job, SceneStatus status, const godot::String& message = {});
};
}
