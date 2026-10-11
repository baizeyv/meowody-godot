#pragma once
#include "runtime/save/model_fields.h"

struct SettingsModel : meowody::Model {
    meowody::Reactive<bool> muted{false};
    meowody::Reactive<double> volume{0.8};
    std::vector<meowody::ModelField> save_fields() override {
        return {
            meowody::model_field("muted", muted),
            meowody::model_field("volume", volume)};
    }
};

#pragma once
#include "runtime/save/save_types.h"
#include "runtime/system/system.h"

class SettingsSystem : public meowody::System {
    SettingsModel state;
    void apply_audio();
    meowody::SaveStatus persist();
public:
    SettingsModel &model() { return state; }
    meowody::SystemStatus initialize() override;
    meowody::SaveStatus restore();
    meowody::SaveStatus set_muted(bool muted);
    meowody::SaveStatus set_volume(double volume);
};

#include "runtime/app.h"
#include <algorithm>
#include <cmath>

meowody::SystemStatus SettingsSystem::initialize() {
    auto restored = restore();
    if (restored.ok() || restored.error == meowody::SaveError::Missing) return {};
    return {meowody::SystemError::InitializationFailed, restored.message.utf8().get_data()};
}
meowody::SaveStatus SettingsSystem::restore() {
    auto status = meowody::App::save->load_model("settings", "default", state);
    // Missing / Corrupt 时保留 Model 的初始值，不覆盖原文件。
    if (status.ok()) state.volume.set(std::clamp(state.volume.get(), 0.0, 1.0));
    apply_audio();
    return status;
}
void SettingsSystem::apply_audio() {
    meowody::App::audio->set_muted("Music", state.muted.get());
    meowody::App::audio->set_volume("Music", float(state.volume.get()));
}
meowody::SaveStatus SettingsSystem::persist() {
    return meowody::App::save->save_model("settings", "default", state);
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

#include "runtime/app.h"
#include "runtime/system/system.h"

class SettingsObserverSystem : public meowody::System {
    std::shared_ptr<SettingsSystem> settings;
    meowody::Connection volume_connection;
    double current_volume = 0.8;
public:
    meowody::SystemStatus initialize() override {
        settings = meowody::App::system->get<SettingsSystem>();
        if (!settings)
            return {meowody::SystemError::InitializationFailed, "SettingsSystem is not registered"};
        current_volume = settings->model().volume.get();
        volume_connection = settings->model().volume.subscribe(
            [this](const double &value) { current_volume = value; });
        return {};
    }
    void shutdown() noexcept override {
        volume_connection.disconnect();
        settings.reset();
        // 在这里取消本 System 拥有的请求、协程和其它任务。
    }
};

void example() {
// 业务启动方法中；不要在静态全局构造中访问 App。
if (!meowody::App::system->register_systems<SettingsSystem, SettingsObserverSystem>()) return;
auto result = meowody::App::system->initialize();
if (!result.ok()) return;
// 此时 SettingsSystem 已恢复数据，观察者已安装订阅，再打开 UI。

}
