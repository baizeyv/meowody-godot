#include "batch_load_handle.h"
#include "detail/batch_state.h"
namespace meowody {
LoadStatus BatchLoadHandle::status() const { return m_state ? m_state->status : LoadStatus::Failed; }
std::size_t BatchLoadHandle::size() const { return m_state ? m_state->items.size() : 0; }
std::size_t BatchLoadHandle::completed_count() const
{
    std::size_t count = 0;
    if (m_state) for (const auto& item : m_state->items) count += item.finished();
    return count;
}
std::size_t BatchLoadHandle::succeeded_count() const
{
    std::size_t count = 0;
    if (m_state) for (const auto& item : m_state->items) count += item.succeeded();
    return count;
}
float BatchLoadHandle::progress() const
{
    if (!m_state || finished() || m_state->items.empty()) return 1.0f;
    float sum = 0;
    for (const auto& item : m_state->items) sum += item.progress();
    return sum / static_cast<float>(m_state->items.size());
}
std::vector<LoadResult<>> BatchLoadHandle::results() const
{
    std::vector<LoadResult<>> results;
    if (m_state)
        for (const auto& item : m_state->items) results.push_back(item.result());
    return results;
}
void BatchLoadHandle::cancel() const
{
    if (!m_state) return;
    m_state->suppress_callback = true;
    if (finished()) return;
    m_state->status = LoadStatus::Cancelled;
    for (const auto& item : m_state->items) item.cancel();
}

}
