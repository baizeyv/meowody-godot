#pragma once
#include "runtime/reactive/reactive.hpp"

struct SettingsModel {
    meowody::Reactive<bool> muted{false};
    meowody::Reactive<double> volume{0.8};
};
#pragma once
#include "runtime/save/save_types.h"

class SettingsSystem {
    SettingsModel state;
    void apply_audio();
    meowody::SaveStatus persist();
public:
    SettingsModel &model() { return state; }
    meowody::SaveResult restore();
    meowody::SaveStatus set_muted(bool muted);
    meowody::SaveStatus set_volume(double volume);
};
#include "runtime/app.h"
#include <algorithm>
#include <cmath>

meowody::SaveResult SettingsSystem::restore() {
    godot::Dictionary defaults;
    defaults["muted"] = false;
    defaults["volume"] = 0.8;
    auto result = meowody::App::save->load("settings", "default", defaults);
    const godot::Variant muted = result.data.get("muted", false);
    const godot::Variant volume = result.data.get("volume", 0.8);
    state.muted.set(muted.get_type() == godot::Variant::BOOL ? bool(muted) : false);
    double value = (volume.get_type() == godot::Variant::FLOAT ||
                    volume.get_type() == godot::Variant::INT) ? double(volume) : 0.8;
    state.volume.set(std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.8);
    apply_audio();
    return result;
}
void SettingsSystem::apply_audio() {
    meowody::App::audio->set_muted("Music", state.muted.get());
    meowody::App::audio->set_volume("Music", float(state.volume.get()));
}
meowody::SaveStatus SettingsSystem::persist() {
    godot::Dictionary data;
    data["muted"] = state.muted.get();
    data["volume"] = state.volume.get();
    return meowody::App::save->save("settings", "default", data);
}
meowody::SaveStatus SettingsSystem::set_muted(bool value) {
    state.muted.set(value);
    apply_audio();
    return persist();
}
meowody::SaveStatus SettingsSystem::set_volume(double value) {
    if (!std::isfinite(value))
        return {meowody::SaveError::InvalidArgument, "Volume must be finite"};
    state.volume.set(std::clamp(value, 0.0, 1.0));
    apply_audio();
    return persist();
}
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/label.hpp>
class SettingsPanelView : public godot::Control {
 GDREFLECT_CLASS(SettingsPanelView, godot::Control)
public:
 static constexpr const char *SCENE_PATH="res://scenes/settings.tscn";
 godot::CheckButton *m_mute_button=nullptr;
 godot::HSlider *m_volume_slider=nullptr;
 godot::Label *m_status_label=nullptr;
void show_settings(bool muted, double volume);
void show_status(const godot::String &text);

};

void SettingsPanelView::show_settings(bool muted, double volume) {
    if (m_mute_button) m_mute_button->set_pressed_no_signal(muted);
    if (m_volume_slider) m_volume_slider->set_value_no_signal(volume);
}
void SettingsPanelView::show_status(const godot::String &text) {
    if (m_status_label) m_status_label->set_text(text);
}
#pragma once
#include "runtime/ui/ui_presenter.h"
#include <memory>

class SettingsPanelView;
class SettingsPresenter final : public meowody::UIPresenter {
    std::shared_ptr<SettingsSystem> system;
    SettingsPanelView *view = nullptr;
    void refresh();
    void report(const meowody::SaveStatus &status);
public:
    explicit SettingsPresenter(std::shared_ptr<SettingsSystem> value);
    void on_attach(godot::Node &root) override;
    void on_open(const godot::Variant &data) override;
    void on_close() override;
};
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

SettingsPresenter::SettingsPresenter(std::shared_ptr<SettingsSystem> value)
    : system(std::move(value)) {}
void SettingsPresenter::on_attach(godot::Node &root) {
    view = godot::Object::cast_to<SettingsPanelView>(&root);
    if (!view || !view->m_mute_button || !view->m_volume_slider || !view->m_status_label) {
        godot::UtilityFunctions::push_error("SettingsPanel scene bindings are incomplete");
        return;
    }
    connect(view->m_mute_button, "toggled", [this](const godot::Array &args) {
        if (!args.is_empty()) report(system->set_muted(bool(args[0])));
    });
    connect(view->m_volume_slider, "value_changed", [this](const godot::Array &args) {
        if (!args.is_empty()) report(system->set_volume(double(args[0])));
    });
    track(system->model().muted.subscribe([this](const bool &) { refresh(); }));
    track(system->model().volume.subscribe([this](const double &) { refresh(); }));
}
void SettingsPresenter::refresh() {
    if (view) view->show_settings(system->model().muted.get(), system->model().volume.get());
}
void SettingsPresenter::report(const meowody::SaveStatus &status) {
    if (view) view->show_status(status.ok() ? godot::String("已暂存，等待帧末提交")
                                          : godot::String("保存失败：") + status.message);
}
void SettingsPresenter::on_open(const godot::Variant &) {
    refresh();
    if (view) view->show_status("");
}
void SettingsPresenter::on_close() {
    view = nullptr; // UICenter 随后会 disconnect，回收所有 track/connect。
}
void doc_registration_example() {
#include "runtime/app.h"

// 以下代码位于业务方法中。
auto settings = std::make_shared<SettingsSystem>();
auto restored = settings->restore();
// restored.used_default 表示使用默认值，按业务决定是否提示存档异常。

meowody::UIOptions options;
options.kind = meowody::UIKind::Window;
options.cache = true;
const bool configured = meowody::App::ui->configure(
    SettingsPanelView::SCENE_PATH, options,
    [settings]() -> std::unique_ptr<meowody::UIPresenter> {
        return std::make_unique<SettingsPresenter>(settings);
    });
if (configured) {
    auto panel = meowody::App::ui->open(SettingsPanelView::SCENE_PATH);
    // panel.is_open() / panel.status() / panel.error() 按业务处理。
}
}
