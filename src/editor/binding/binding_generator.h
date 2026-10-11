#pragma once
#include <godot_cpp/classes/node.hpp>
#include <vector>
namespace meowody {
inline constexpr const char *BIND_MARK = "meowody_bind";
inline constexpr const char *BIND_CLASS = "meowody_bind_class";
inline constexpr const char *BIND_BASE = "meowody_bind_base";
inline constexpr const char *BIND_OUTPUT = "meowody_bind_output";
struct NativeBinding {
	godot::Node *node = nullptr;
	godot::String member, cpp_type, header;
};
struct BindingResult {
	bool ok = false;
	godot::String error;
};
class BindingGenerator {
public:
	static godot::String default_class_name(godot::Node *);
	static godot::String member_name(const godot::String &);
	static bool valid_class_name(const godot::String &);
	static std::vector<NativeBinding> collect(godot::Node *, godot::String &error);
	static BindingResult generate(godot::Node *);
};
} //namespace meowody
