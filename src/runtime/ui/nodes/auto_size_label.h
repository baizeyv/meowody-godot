#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class AutoSizeLabel : public godot::Label {
    GDREFLECT_CLASS(AutoSizeLabel, godot::Label)
    GDPROPERTY_RANGE(1, 512, 1) int min_font_size = 12;
    GDPROPERTY_RANGE(1, 512, 1) int max_font_size = 40;
    GDPROPERTY bool fit_width = true;
    GDPROPERTY bool fit_height = true;
    godot::Array state;
    godot::Ref<godot::Font> observed;
    bool dirty = true;
    void font_changed();
public:
    AutoSizeLabel();
    ~AutoSizeLabel() override;
    void _process(double delta) override;
    void _notification(int what);
    GDMETHOD void fit_now();
    GDMETHOD int get_fitted_font_size() const;
};
}
