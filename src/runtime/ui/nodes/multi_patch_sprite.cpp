#include "multi_patch_sprite.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace meowody {
using namespace godot;
namespace {
constexpr size_t MAX_PATCHES = 32768;
struct Span { double source, source_size, destination, destination_size; };
int mode_at(const PackedInt32Array &modes, int i, int count) {
    return modes.is_empty() ? (count > 1 && (i == 0 || i == count - 1) ? 0 : 1) : modes[i];
}
String validate_axis(int extent, const PackedInt32Array &cuts, const PackedInt32Array &modes, const PackedFloat32Array &weights) {
    if (cuts.size() > 64) return "At most 64 cuts per axis are supported.";
    int previous = 0;
    for (int i = 0; i < cuts.size(); ++i) {
        if (cuts[i] <= previous || cuts[i] >= extent) return "Cuts must be strictly increasing and inside the sprite region.";
        previous = cuts[i];
    }
    if (!modes.is_empty() && modes.size() != cuts.size() + 1) return "Modes must contain one value per segment.";
    for (int i = 0; i < modes.size(); ++i) if (modes[i] < 0 || modes[i] > 3) return "Invalid segment mode.";
    if (!weights.is_empty() && weights.size() != cuts.size() + 1) return "Weights must contain one value per segment.";
    for (int i = 0; i < weights.size(); ++i) if (!std::isfinite(weights[i]) || weights[i] <= 0) return "Weights must be finite and positive.";
    return {};
}
double fixed_size(int extent, const PackedInt32Array &cuts, const PackedInt32Array &modes) {
    double fixed = 0;
    for (int i = 0, start = 0; i <= cuts.size(); ++i) {
        int end = i < cuts.size() ? cuts[i] : extent;
        if (mode_at(modes, i, cuts.size() + 1) == 0) fixed += end - start;
        start = end;
    }
    return fixed;
}
std::vector<Span> axis(int extent, double target, const PackedInt32Array &cuts, const PackedInt32Array &modes, const PackedFloat32Array &weights) {
    std::vector<Span> result;
    double fixed = fixed_size(extent, cuts, modes), total_weight = 0;
    const int count = cuts.size() + 1;
    for (int i = 0; i < count; ++i) if (mode_at(modes, i, count) != 0) total_weight += weights.is_empty() ? 1 : weights[i];
    double position = 0;
    int start = 0;
    for (int i = 0; i < count; ++i) {
        const int end = i < cuts.size() ? cuts[i] : extent;
        const double length = end - start;
        const int mode = mode_at(modes, i, count);
        double destination = mode == 0 ? length * (fixed > target ? target / fixed : 1) :
            std::max(0.0, target - fixed) * (weights.is_empty() ? 1 : weights[i]) / total_weight;
        if (destination > 0 && length > 0) {
            if (mode < 2) result.push_back({double(start), length, position, destination});
            else {
                const double repeats = mode == 3 ? std::max(1.0, std::round(destination / length)) : std::ceil(destination / length);
                if (repeats > MAX_PATCHES || result.size() + repeats > MAX_PATCHES) return {};
                const double step = mode == 3 ? destination / repeats : length;
                for (int tile = 0; tile < int(repeats); ++tile) {
                    double width = std::min(step, destination - tile * step);
                    result.push_back({double(start), mode == 3 ? length : width, position + tile * step, width});
                }
            }
        }
        position += destination;
        start = end;
    }
    return result;
}
}

MultiPatchSprite::~MultiPatchSprite() {
    if (texture.is_valid() && texture->is_connected("changed", callable_mp(this, &MultiPatchSprite::texture_changed)))
        texture->disconnect("changed", callable_mp(this, &MultiPatchSprite::texture_changed));
}
void MultiPatchSprite::texture_changed() { emit_changed(); }
void MultiPatchSprite::set_texture(const Ref<Texture2D> &value) {
    ERR_FAIL_COND_MSG(value.is_valid() && value->is_class("MultiPatchTexture"), "MultiPatchSprite requires an original texture or AtlasTexture source.");
    if (texture == value) return;
    if (texture.is_valid()) texture->disconnect("changed", callable_mp(this, &MultiPatchSprite::texture_changed));
    texture = value;
    if (texture.is_valid()) texture->connect("changed", callable_mp(this, &MultiPatchSprite::texture_changed));
    emit_changed();
}
void MultiPatchSprite::set_region(const Rect2i &value) { region = value; emit_changed(); }
#define PATCH_SETTER(Type, Name) void MultiPatchSprite::set_##Name(const Type &value) { Name = value; emit_changed(); }
PATCH_SETTER(PackedInt32Array, x_cuts)
PATCH_SETTER(PackedInt32Array, y_cuts)
PATCH_SETTER(PackedInt32Array, x_modes)
PATCH_SETTER(PackedInt32Array, y_modes)
PATCH_SETTER(PackedFloat32Array, x_weights)
PATCH_SETTER(PackedFloat32Array, y_weights)
#undef PATCH_SETTER

Rect2i MultiPatchSprite::get_effective_region() const {
    if (texture.is_null()) return {};
    return region == Rect2i() ? Rect2i(Vector2i(), Vector2i(texture->get_size())) : region;
}
String MultiPatchSprite::get_validation_error() const {
    if (texture.is_null()) return "Assign a source texture.";
    // A patch texture cannot itself be used as patch data: that would form a drawing cycle.
    if (texture->is_class("MultiPatchTexture")) return "Use an original texture or AtlasTexture as the source.";
    Rect2i area = get_effective_region();
    if (area.size.x <= 0 || area.size.y <= 0 || !Rect2i(Vector2i(), Vector2i(texture->get_size())).encloses(area)) return "Sprite region must be inside the source texture.";
    String error = validate_axis(area.size.x, x_cuts, x_modes, x_weights);
    if (!error.is_empty()) return "X: " + error;
    error = validate_axis(area.size.y, y_cuts, y_modes, y_weights);
    return error.is_empty() ? error : "Y: " + error;
}
Vector2 MultiPatchSprite::get_minimum_size() const {
    if (!get_validation_error().is_empty()) return {};
    auto size = get_effective_region().size;
    return Vector2(fixed_size(size.x, x_cuts, x_modes), fixed_size(size.y, y_cuts, y_modes));
}
std::vector<MultiPatchSprite::Patch> MultiPatchSprite::layout(const Vector2 &size) const {
    std::vector<Patch> patches;
    if (!get_validation_error().is_empty() || !std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0 || size.y <= 0) return patches;
    auto region = get_effective_region();
    auto xs = axis(region.size.x, size.x, x_cuts, x_modes, x_weights);
    auto ys = axis(region.size.y, size.y, y_cuts, y_modes, y_weights);
    if (xs.size() * ys.size() > MAX_PATCHES) return patches;
    patches.reserve(xs.size() * ys.size());
    for (const auto &y : ys) for (const auto &x : xs)
        patches.push_back({Rect2(region.position.x + x.source, region.position.y + y.source, x.source_size, y.source_size),
                           Rect2(x.destination, y.destination, x.destination_size, y.destination_size)});
    return patches;
}
Array MultiPatchSprite::get_draw_regions(const Vector2 &size) const {
    Array result;
    for (const auto &patch : layout(size)) {
        Dictionary item;
        item["source"] = patch.source;
        item["destination"] = patch.destination;
        result.append(item);
    }
    return result;
}
void MultiPatchSprite::draw_to(const RID &canvas, const Rect2 &rect, const Color &tint, bool transpose, const Rect2 &clip) const {
    Vector2 size(std::abs(rect.size.x), std::abs(rect.size.y));
    if (transpose) std::swap(size.x, size.y);
    for (auto patch : layout(size)) {
        Rect2 dest = patch.destination;
        if (transpose) {
            std::swap(dest.position.x, dest.position.y);
            std::swap(dest.size.x, dest.size.y);
        }
        // Godot flips negative-sized texture rects inside the same positive bounds.
        if (rect.size.x < 0) dest.position.x = std::abs(rect.size.x) - dest.position.x - dest.size.x;
        if (rect.size.y < 0) dest.position.y = std::abs(rect.size.y) - dest.position.y - dest.size.y;
        dest.position += rect.position;
        if (clip.has_area()) {
            Rect2 intersection = dest.intersection(clip);
            if (!intersection.has_area()) continue;
            Vector2 from = (intersection.position - dest.position) / dest.size;
            Vector2 fraction = intersection.size / dest.size;
            if (rect.size.x < 0) from.x = 1 - from.x - fraction.x;
            if (rect.size.y < 0) from.y = 1 - from.y - fraction.y;
            if (transpose) { std::swap(from.x, from.y); std::swap(fraction.x, fraction.y); }
            patch.source.position += patch.source.size * from;
            patch.source.size *= fraction;
            dest = intersection;
        }
        if (rect.size.x < 0) dest.size.x = -dest.size.x;
        if (rect.size.y < 0) dest.size.y = -dest.size.y;
        // This virtual call preserves AtlasTexture regions and transparent trimming margins.
        texture->draw_rect_region(canvas, dest, patch.source, tint, transpose, true);
    }
}
}
