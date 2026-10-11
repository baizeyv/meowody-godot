#pragma once
#include "time_types.h"
#include <memory>
namespace meowody {
namespace detail {
struct SyncState { TimeSyncStatus status = TimeSyncStatus::Pending; TimeSyncResult result; };
struct DailyState { uint64_t id = 0; bool cancelled = false; bool paused = false; bool restart = false; };
}
class TimeCenter;
class TimeSyncHandle {
public:
    TimeSyncHandle() = default;
    bool valid() const { return bool(m_state); }
    TimeSyncStatus status() const { return m_state ? m_state->status : TimeSyncStatus::Cancelled; }
    bool is_finished() const { auto s = status(); return s == TimeSyncStatus::Succeeded || s == TimeSyncStatus::Failed || s == TimeSyncStatus::Cancelled; }
    bool succeeded() const { return status() == TimeSyncStatus::Succeeded; }
    TimeSyncResult result() const {
        if (!is_finished()) { TimeSyncResult r; r.error = TimeSyncError::NotReady; r.message = "Synchronization has not finished"; return r; }
        if (m_state) return m_state->result;
        TimeSyncResult r; r.error = TimeSyncError::InvalidArgument; r.message = "Invalid handle"; return r;
    }
    void cancel() {
        if (!m_state) return;
        m_suppress();
        if (!is_finished()) { m_state->status = TimeSyncStatus::Cancelled; m_state->result.error = TimeSyncError::Cancelled; m_state->result.message = "Cancelled"; }
    }
private:
    friend class TimeCenter;
    std::shared_ptr<detail::SyncState> m_state;
    // Cancellation also suppresses terminal callbacks still waiting for delivery.
    std::shared_ptr<bool> m_cancelled;
    void m_suppress() { if (m_cancelled) *m_cancelled = true; }
};
class DailySubscription {
public:
    DailySubscription() = default;
    uint64_t id() const { return m_state ? m_state->id : 0; }
    bool is_active() const { return m_state && !m_state->cancelled; }
    bool is_paused() const { return m_state && m_state->paused; }
    void cancel() { if (m_state) m_state->cancelled = true; }
    void pause() { if (is_active()) m_state->paused = true; }
    void resume() { if (is_active() && m_state->paused) { m_state->paused = false; m_state->restart = true; } }
private:
    friend class TimeCenter;
    std::shared_ptr<detail::DailyState> m_state;
};
}
