#include "debug_editor_backend.h"
#include "log/log.hpp"
#include "runtime/debug/debug_types.h"
#include "runtime/debug/debug_log_style.h"
#include <mutex>

namespace meowody {
using namespace godot;
namespace {
DebugLogBuffer pending;
Connection subscription;
std::mutex mutex;
}
void DebugEditorBackend::_bind_methods() {
    ClassDB::bind_method(D_METHOD("drain"), &DebugEditorBackend::drain);
    ClassDB::bind_method(D_METHOD("spans", "text", "rich"), &DebugEditorBackend::spans);
    ClassDB::bind_method(D_METHOD("write_test_log"), &DebugEditorBackend::write_test_log);
}
void DebugEditorBackend::initialize() {
    subscription = Log::subscribe([](const LogRecord &record) {
        std::lock_guard lock(mutex);
        pending.push(record);
    });
}
void DebugEditorBackend::shutdown() {
    subscription.disconnect();
    std::lock_guard lock(mutex);
    pending.clear();
}
Array DebugEditorBackend::drain() {
    std::lock_guard lock(mutex);
    Array result;
    for (const auto &r : pending.records()) {
        Array row;
        row.push_back(int64_t(r.sequence)); row.push_back(r.timestamp);
        row.push_back(r.level); row.push_back(r.tag); row.push_back(r.text);
        row.push_back(r.stack); row.push_back(r.rich);
        result.push_back(row);
    }
    pending.clear();
    return result;
}
Array DebugEditorBackend::spans(const String &text, bool rich) {
    LogRecord record; record.text = text; record.rich = rich;
    Array result;
    for (const auto &span : debug_log_spans(record)) {
        Dictionary value; value["text"] = span.text; value["color"] = span.color;
        result.push_back(value);
    }
    return result;
}
void DebugEditorBackend::write_test_log() {
    Log::info(StringName("EditorTest")) << "Editor local log";
}
}
