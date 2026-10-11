#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>

namespace meowody {
class DebugEditorBackend : public godot::RefCounted {
    GDCLASS(DebugEditorBackend, godot::RefCounted)
protected:
    static void _bind_methods();
public:
    static void initialize();
    static void shutdown();
    godot::Array drain();
    godot::Array spans(const godot::String &text, bool rich);
    void write_test_log();
};
}
