#pragma once

#include <memory>
#include "detail/load_state.h"

namespace meowody
{
template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
class LoadHandle
{
public:
    LoadHandle() = default;
    bool valid() const { return static_cast<bool>(m_state); }
    LoadStatus status() const { return m_state ? m_state->status : LoadStatus::Failed; }
    bool finished() const { return is_load_finished(status()); }
    float progress() const { return m_state ? m_state->progress : 1.0f; }
    float progress_percent() const { return valid() ? progress() * 100.0f : 0.0f; }
    bool is_finished() const { return finished(); }
    bool succeeded() const { return status() == LoadStatus::Succeeded; }
    bool failed() const { return status() == LoadStatus::Failed; }
    bool cancelled() const { return status() == LoadStatus::Cancelled; }
    LoadResult<T> result() const
    {
        if (!m_state) return {{}, LoadError::InvalidHandle, godot::OK, "Invalid load handle"};
        return detail::typed_result<T>(m_state->result);
    }
    void cancel() const { if (m_state) m_state->cancel(); }

private:
    friend class ResourceCenter;
    explicit LoadHandle(std::shared_ptr<detail::LoadState> state) : m_state(std::move(state)) {}
    std::shared_ptr<detail::LoadState> m_state;
};
}
