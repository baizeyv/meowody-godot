#include "auto_size_label.h"
#include "node_helpers.h"
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
AutoSizeLabel::AutoSizeLabel() { set_process(true); set_clip_text(true); }
AutoSizeLabel::~AutoSizeLabel() { if (observed.is_valid()) observed->disconnect("changed", callable_mp(this, &AutoSizeLabel::font_changed)); }
void AutoSizeLabel::font_changed() { dirty = true; }
void AutoSizeLabel::_notification(int what) { if (what == NOTIFICATION_THEME_CHANGED || what == NOTIFICATION_TRANSLATION_CHANGED || what == NOTIFICATION_RESIZED) dirty = true; }
int AutoSizeLabel::get_fitted_font_size() const { return get_theme_font_size("font_size"); }
void AutoSizeLabel::_process(double) {
    auto font = get_theme_font("font");
    if (font != observed) {
        if (observed.is_valid()) observed->disconnect("changed", callable_mp(this, &AutoSizeLabel::font_changed));
        observed = font;
        if (observed.is_valid()) observed->connect("changed", callable_mp(this, &AutoSizeLabel::font_changed));
        dirty = true;
    }
    if (changed(state, {get_text(), get_size(), min_font_size, max_font_size, fit_width, fit_height, int(get_autowrap_mode()), int(get_text_direction()), get_language()})) dirty = true;
    if (dirty) { dirty = false; fit_now(); dirty = false; }
}
void AutoSizeLabel::fit_now() {
    Ref<Font> font = get_theme_font("font");
    if (font.is_null() || get_size().x <= 0 || get_size().y <= 0) return;
    int low = std::clamp(min_font_size,1,512), high = std::clamp(max_font_size, low,512), best = low;
    int flags = TextServer::BREAK_MANDATORY;
    if (get_autowrap_mode() == TextServer::AUTOWRAP_ARBITRARY) flags |= TextServer::BREAK_GRAPHEME_BOUND;
    if (get_autowrap_mode() == TextServer::AUTOWRAP_WORD) flags |= TextServer::BREAK_WORD_BOUND;
    if (get_autowrap_mode() == TextServer::AUTOWRAP_WORD_SMART) flags |= TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE;
    while (low <= high) {
        int mid = (low + high) / 2;
        add_theme_font_size_override("font_size", mid);
        Vector2 measured = font->get_multiline_string_size(tr(get_text()), HORIZONTAL_ALIGNMENT_LEFT, get_autowrap_mode() == TextServer::AUTOWRAP_OFF ? -1 : get_size().x, mid, -1, BitField<TextServer::LineBreakFlag>(flags));
        // Label includes fallback-font heights and theme line spacing which Font's
        // aggregate string measurement alone does not always account for.
        measured.y = 0;
        for (int line=0; line<get_line_count(); ++line) measured.y += get_line_height(line);
        measured.y += std::max(0, get_line_count()-1) * get_theme_constant("line_spacing");
        if ((!fit_width || measured.x <= get_size().x + 0.5) && (!fit_height || measured.y <= get_size().y + 0.5)) { best = mid; low = mid + 1; }
        else high = mid - 1;
    }
    if (get_theme_font_size("font_size") != best) add_theme_font_size_override("font_size", best);
}}
