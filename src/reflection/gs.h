#pragma once

#include "annotations.h"

#define GS(Type, Name)                       \
private:                                     \
    GDPROPERTY_NAME(#Name)                  \
    GDPROPERTY_ACCESSORS(get_##Name, set_##Name) \
    Type Name##_{};               \
public:                                      \
    const Type &get_##Name() const { \
        return Name##_;                      \
    }                                        \
    void set_##Name(const Type &value) { \
        Name##_ = value;                     \
    }
