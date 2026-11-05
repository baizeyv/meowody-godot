#pragma once
#include "load_types.h"
#include <memory>
#include <cstdint>

namespace meowody {
enum class ResourceMode { Auto, Source, Packed };
struct ResourceOptions {
    ResourceMode mode = ResourceMode::Auto;
    godot::String settings_path = "res://resources/settings.json";
    godot::String catalog_path = "res://resources/catalog.json";
    // Selected catalog snapshots. Pack bytes use the injected DownloadCenter's cache.
    godot::String cache_directory = "user://meowody/resources";
};
namespace detail {
struct ResourceOperationState {
    LoadStatus status = LoadStatus::Pending;
    LoadError error = LoadError::None;
    godot::String message;
    std::int64_t downloaded_bytes = 0, total_bytes = 0;
    bool fallback = false;
};
}
class ResourceOperation {
public:
    ResourceOperation() = default;
    explicit ResourceOperation(std::shared_ptr<detail::ResourceOperationState> state) : m_state(std::move(state)) {}
    bool valid() const { return bool(m_state); }
    LoadStatus status() const { return m_state ? m_state->status : LoadStatus::Failed; }
    bool finished() const { return is_load_finished(status()); }
    bool succeeded() const { return status() == LoadStatus::Succeeded; }
    LoadError error() const { return m_state ? m_state->error : LoadError::InvalidHandle; }
    godot::String message() const { return m_state ? m_state->message : godot::String("Invalid resource operation"); }
    bool used_fallback() const { return m_state && m_state->fallback; }
    std::int64_t downloaded_bytes() const { return m_state ? m_state->downloaded_bytes : 0; }
    std::int64_t total_bytes() const { return m_state ? m_state->total_bytes : 0; }
    float progress() const { return finished() ? 1.0f : total_bytes() > 0 ? float(double(downloaded_bytes()) / total_bytes()) : 0.0f; }
    void cancel() const { if (m_state && !finished()) { m_state->status = LoadStatus::Cancelled; m_state->error = LoadError::Cancelled; } }
private:
    std::shared_ptr<detail::ResourceOperationState> m_state;
};
}
