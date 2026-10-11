#pragma once

#include <memory>
#include <vector>
#include "load_handle.h"

namespace meowody
{
namespace detail { struct BatchState; }
class BatchLoadHandle
{
public:
    BatchLoadHandle() = default;
    bool valid() const { return static_cast<bool>(m_state); }
    LoadStatus status() const;
    bool finished() const { return is_load_finished(status()); }
    float progress() const;
    float progress_percent() const { return valid() ? progress() * 100.0f : 0.0f; }
    bool is_finished() const { return finished(); }
    bool succeeded() const { return status() == LoadStatus::Succeeded; }
    bool failed() const { return status() == LoadStatus::Failed; }
    bool cancelled() const { return status() == LoadStatus::Cancelled; }
    std::size_t size() const;
    std::size_t completed_count() const;
    std::size_t succeeded_count() const;
    std::vector<LoadResult<>> results() const;
    void cancel() const;

private:
    friend class ResourceCenter;
    explicit BatchLoadHandle(std::shared_ptr<detail::BatchState> state) : m_state(std::move(state)) {}
    std::shared_ptr<detail::BatchState> m_state;
};
}
