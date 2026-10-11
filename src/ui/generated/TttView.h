#pragma once
#include "TttView.includes.gen.h"
#include "reflection/annotations.h"
#include "reflection/class_macros.h"

class TttView : public godot::Control {
    GDREFLECT_CLASS(TttView, godot::Control)
public:
#include "TttView.bindings.gen.inc"
};
