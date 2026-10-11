#pragma once

#include <godot_cpp/classes/wrapped.hpp>
#include "annotations.h"

#if !defined(GDREFLECT_CODEGEN) && __has_include("gdreflect_decls.gen.h")
#include "gdreflect_decls.gen.h"
#endif

#ifndef GDREFLECT_CLASS_GENERATED_MEMBERS
#define GDREFLECT_CLASS_GENERATED_MEMBERS(ClassName)
#endif

#define GDREFLECT_CLASS(ClassName, BaseName) \
    GDCLASS(ClassName, BaseName)             \
protected:                                   \
    GDREFLECT_ANNOTATE("gdclass_marker") static void _bind_methods(); \
public:                                      \
    GDREFLECT_CLASS_GENERATED_MEMBERS(ClassName)
