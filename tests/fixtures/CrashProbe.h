#pragma once
#include "reflection/class_macros.h"
#include <godot_cpp/classes/ref_counted.hpp>
class CrashProbe : public godot::RefCounted {
    GDREFLECT_CLASS(CrashProbe, godot::RefCounted)
public:
    GDMETHOD void crash();
    GDMETHOD bool handled_fault();
};
