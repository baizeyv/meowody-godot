#pragma once
#include "save_types.h"
#include "runtime/core/delegate.hpp"
#include <godot_cpp/variant/packed_string_array.hpp>
#include <type_traits>
#include <map>

namespace meowody {
class Model;
class SaveCenter {
public:
    explicit SaveCenter(godot::String directory = "user://meowody/saves", SaveOptions options = {});
    ~SaveCenter();
    SaveCenter(const SaveCenter&) = delete;
    SaveCenter& operator=(const SaveCenter&) = delete;
    SaveStatus configure(godot::String directory, SaveOptions options = {});
    SaveStatus save(const godot::String& part, const godot::String& slot, const godot::Dictionary& data);
    SaveResult load(const godot::String& part, const godot::String& slot, const godot::Dictionary& defaults = {}) const;
    SaveStatus save_object(const godot::String& part, const godot::String& slot, godot::Object* object);
    template<class T> SaveStatus save_resource(const godot::String& part, const godot::String& slot, const godot::Ref<T>& resource) {
        static_assert(std::is_base_of_v<godot::Resource, T>);
        return save_object(part, slot, resource.ptr());
    }
    // Target must contain initial data; validation failures leave it unchanged.
    SaveStatus load_into(const godot::String& part, const godot::String& slot, godot::Object* initial_object) const;
    template<class T> SavedResource<T> load_resource(const godot::String& part, const godot::String& slot,
            Delegate<godot::Ref<T>()> defaults = {}) const {
        static_assert(std::is_base_of_v<godot::Resource, T>);
        SavedResource<T> result;
        if (defaults) result.data = defaults(); else result.data.instantiate();
        auto status = load_into(part, slot, result.data.ptr());
        result.error = status.error; result.message = status.message; result.used_default = !status.ok();
        return result;
    }
    SaveStatus save_model(const godot::String& part, const godot::String& slot, Model& model);
    ModelLoadResult load_model(const godot::String& part, const godot::String& slot, Model& model) const;
    bool has_slot(const godot::String& part, const godot::String& slot) const;
    godot::PackedStringArray list_slots(const godot::String& part) const;
    godot::PackedStringArray list_parts() const;
    // Read-only diagnostics: no defaults, warning log, flush or mutation.
    SaveResult inspect(const godot::String& part, bool disk = false) const;
    bool is_pending(const godot::String& part) const { return m_pending.contains(part.to_lower()); }
    SaveStatus erase_slot(const godot::String& part, const godot::String& slot);
    // Explicitly replaces even a damaged partition with an empty, valid container.
    SaveStatus reset_part(const godot::String& part);
    godot::String part_path(const godot::String& part) const;
    godot::String directory() const { return m_directory; }
    SaveStatus flush_pending(); // Explicit durable commit; may be called outside frame driving.
    SaveStatus flush_frame(uint64_t frame); // At most one commit per partition and frame.
    void request_flush();
    void set_flush_scheduler(Delegate<void()> scheduler) { m_scheduler = std::move(scheduler); }
    size_t pending_count() const { return m_pending.size(); }
    uint64_t commit_count() const { return m_commits; }
    SaveStatus last_flush_status() const { return m_last_flush; }
    void shutdown();
    bool is_closed() const { return m_closed; }
private:
    godot::String m_directory;
    SaveOptions m_options;
    bool m_closed = false;
    bool m_scheduled = false;
    uint64_t m_commits = 0;
    SaveStatus m_last_flush;
    Delegate<void()> m_scheduler;
    std::map<godot::String, godot::Dictionary> m_pending;
    std::map<godot::String, uint64_t> m_last_frame;
    SaveStatus stage_part(const godot::String& part, const godot::Dictionary& slots);
    SaveStatus flush(bool per_frame, uint64_t frame);
    SaveStatus validate(const godot::String& part, const godot::String& slot = "default") const;
    SaveResult read_part(const godot::String& part, bool deep_copy_pending = true) const;
    SaveResult read_file(const godot::String& path) const;
    SaveStatus write_part(const godot::String& part, const godot::Dictionary& slots);
};
}
