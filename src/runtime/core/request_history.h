#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <godot_cpp/variant/string.hpp>

namespace meowody::detail
{
// Progress-only history: weak ownership never pins completed resources or callbacks.
template <typename State, typename Progress>
class RequestHistory
{
public:
    using Snapshot = Progress (*)(const State&);
    explicit RequestHistory(Snapshot snapshot) : m_snapshot(snapshot) {}
    void track(const godot::String& key, const std::shared_ptr<State>& state)
    {
        store(key, {state, m_snapshot(*state), ++m_sequence});
    }
    void remember(const godot::String& key, const Progress& progress)
    {
        store(key, {{}, progress, ++m_sequence});
    }
    Progress query(const godot::String& key) const
    {
        const auto found = m_entries.find(key);
        if (found == m_entries.end()) return {};
        if (auto state = found->second.state.lock()) return m_snapshot(*state);
        return found->second.last;
    }
    void refresh()
    {
        for (auto& [key, entry] : m_entries)
            if (auto state = entry.state.lock())
            {
                const bool was_finished = entry.last.finished();
                entry.last = m_snapshot(*state);
                m_completed += entry.last.finished();
                m_completed -= was_finished;
            }
        prune();
    }
    void clear() { m_entries.clear(); m_completed = 0; }
private:
    struct Entry { std::weak_ptr<State> state; Progress last; std::uint64_t sequence = 0; };
    void store(const godot::String& key, Entry entry)
    {
        auto [it, inserted] = m_entries.try_emplace(key);
        if (!inserted) m_completed -= it->second.last.finished();
        it->second = std::move(entry);
        m_completed += it->second.last.finished();
        prune();
    }
    void prune()
    {
        // Retain all active keys, but at most 128 completed keys.
        if (m_completed <= 128) return;
        std::vector<typename decltype(m_entries)::iterator> completed;
        completed.reserve(m_completed);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            if (it->second.last.finished()) completed.push_back(it);
        const auto excess = m_completed - 128;
        // Partition iterator copies only: the key order and retained snapshots stay unchanged.
        std::nth_element(completed.begin(), completed.begin() + excess, completed.end(),
            [](const auto& a, const auto& b) { return a->second.sequence < b->second.sequence; });
        for (std::size_t i = 0; i < excess; ++i) m_entries.erase(completed[i]);
        m_completed = 128;
    }
    Snapshot m_snapshot;
    std::uint64_t m_sequence = 0;
    std::size_t m_completed = 0;
    std::map<godot::String, Entry> m_entries;
};
}
