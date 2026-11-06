#pragma once
#include "reflection/reflection.h"
#include "runtime/core/connection.hpp"
#include "runtime/task/task_center.h"
#include "creator_backend.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/option_button.hpp>

class ProjectCreatorView : public godot::Control {
    GDREFLECT_CLASS(ProjectCreatorView, godot::Control)
public:
    void _ready() override;
    void _process(double delta) override;
    GDMETHOD
    void run_operation(godot::String operation);
    GDMETHOD
    void browse(bool engine);
    GDMETHOD
    void selected(godot::String path);
    GDMETHOD
    void open_project();
    GDMETHOD
    void open_folder();
    GDMETHOD
    void mode_changed(bool use_local);
    GDMETHOD
    void engine_chosen(int index);
    GDMETHOD
    godot::Dictionary generate_project(godot::Dictionary request);
private:
    godot::LineEdit *name = nullptr, *destination = nullptr, *ref = nullptr, *engine = nullptr;
    godot::OptionButton *engines = nullptr, *versions = nullptr;
    godot::CheckBox *local = nullptr;
    godot::Label *status = nullptr;
    godot::TextEdit *log = nullptr;
    godot::FileDialog *picker = nullptr;
    godot::Control *engine_fields = nullptr, *version_fields = nullptr;
    godot::ProgressBar *progress = nullptr;
    std::vector<godot::Button *> actions;
    godot::Button *open = nullptr;
    meowody::Connection quit_connection;
    meowody::TaskHandle task;
    bool busy = false, choosing_engine = false;
    godot::String operation, log_path, created, executable, previous_engine, saved_version;
    creator::Job active_job;
    int64_t log_length = -1;
    void finished(const meowody::TaskResult &result);
    void set_busy(bool value);
    void start_job(creator::Job job);
};
