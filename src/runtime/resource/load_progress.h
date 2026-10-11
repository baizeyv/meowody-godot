#pragma once
#include "detail/batch_state.h"
#include <algorithm>

namespace meowody
{
struct LoadProgress
{
    bool found = false;
    LoadStatus status = LoadStatus::Failed;
    LoadError error = LoadError::InvalidHandle;
    float progress = 0.0f;
    bool finished() const { return found && is_load_finished(status); }
    bool succeeded() const { return found && status == LoadStatus::Succeeded; }
    float progress_percent() const { return found ? std::clamp(progress * 100.0f, 0.0f, 100.0f) : 0.0f; }
    static LoadProgress snapshot(const detail::LoadState& state)
    { return {true, state.status, state.result.error, state.progress}; }
    static LoadProgress batch_snapshot(const detail::BatchState& state)
    {
        float progress = 0.0f;
        for (const auto& item : state.items) progress += item.progress();
        progress = is_load_finished(state.status) || state.items.empty() ? 1.0f : progress / state.items.size();
        const auto error = state.status == LoadStatus::Failed ? LoadError::LoadFailed :
            state.status == LoadStatus::Cancelled ? LoadError::Cancelled : LoadError::None;
        return {true, state.status, error, progress};
    }
};
}
