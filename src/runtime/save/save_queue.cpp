#include "save_center.h"
#include "log/log.hpp"
#include <godot_cpp/variant/utility_functions.hpp>
namespace meowody {
SaveCenter::~SaveCenter() { shutdown(); }
SaveStatus SaveCenter::stage_part(const godot::String& part, const godot::Dictionary& slots) {
    auto status = validate(part); if (!status.ok()) return status;
    if (!m_options.defer_writes) return write_part(part, slots);
    if (godot::UtilityFunctions::var_to_bytes(slots).size() + 44 > m_options.max_file_bytes)
        return {SaveError::InvalidArgument, "Partition exceeds max_file_bytes"};
    // Private callers own the top-level slots dictionary and snapshot new payloads.
    // Unmodified slots share immutable internal snapshots; public reads still deep-copy.
    m_pending[part.to_lower()] = slots;
    request_flush(); return {};
}
void SaveCenter::request_flush() {
    if (m_closed || m_pending.empty() || m_scheduled || !m_scheduler) return;
    m_scheduled = true; m_scheduler();
}
SaveStatus SaveCenter::flush(bool per_frame, uint64_t frame) {
    if (m_closed) return {SaveError::Closed, "SaveCenter is closed"};
    m_scheduled = false; m_last_flush = {};
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        auto last = m_last_frame.find(it->first);
        if (per_frame && last != m_last_frame.end() && last->second == frame) { ++it; continue; }
        auto status = write_part(it->first, it->second);
        if (per_frame) m_last_frame[it->first] = frame;
        if (status.ok()) it = m_pending.erase(it);
        else {
            m_last_flush = status;
            Log::warn(godot::StringName("SaveCenter")) << it->first << ": " << status.message << "; pending data retained";
            ++it;
        }
    }
    return m_last_flush;
}
SaveStatus SaveCenter::flush_pending() { return flush(false, 0); }
SaveStatus SaveCenter::flush_frame(uint64_t frame) { return flush(true, frame); }
void SaveCenter::shutdown() {
    if (m_closed) return;
    flush_pending(); m_closed = true; m_scheduler.reset();
}
}
