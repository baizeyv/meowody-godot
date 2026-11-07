#include "project_creator_view.h"
#include "runtime/app.h"
#include "project_template.h"
#include <algorithm>
#include <regex>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/object.hpp>
using namespace godot;
using namespace meowody;

namespace {
// Catppuccin Mocha: Text, Subtext0 and Mauve. Keep DLL initialization engine-independent.
constexpr Color INK(205.f / 255, 214.f / 255, 244.f / 255), MUTED(166.f / 255, 173.f / 255, 200.f / 255), ACCENT(203.f / 255, 166.f / 255, 247.f / 255);

Ref<StyleBoxFlat> box(const Color &color, const Color &border, int radius = 10, int padding = 14) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(color);
    style->set_border_color(border);
    style->set_border_width_all(1);
    style->set_corner_radius_all(radius);
    style->set_content_margin_all(padding);
    return style;
}

Label *label(Node *parent, const String &text, int size = 15, const Color &color = INK) {
    auto *node = memnew(Label);
    node->set_text(text);
    node->add_theme_font_size_override("font_size", size);
    node->add_theme_color_override("font_color", color);
    parent->add_child(node);
    return node;
}
VBoxContainer *column(Node *parent, int separation = 12) {
    auto *node = memnew(VBoxContainer);
    node->add_theme_constant_override("separation", separation);
    node->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    parent->add_child(node);
    return node;
}
HBoxContainer *row(Node *parent, int separation = 12) {
    auto *node = memnew(HBoxContainer);
    node->add_theme_constant_override("separation", separation);
    parent->add_child(node);
    return node;
}
VBoxContainer *card(Node *parent) {
    auto *panel = memnew(PanelContainer);
    panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    panel->add_theme_stylebox_override("panel", box(Color("1e1e2e"), Color("45475a"), 16, 22));
    parent->add_child(panel);
    return column(panel);
}
LineEdit *field(Node *parent, const char *id, const String &title, const String &value) {
    label(parent, title, 13, MUTED);
    auto *node = memnew(LineEdit);
    node->set_name(id);
    node->set_text(value);
    node->set_custom_minimum_size(Vector2(0, 44));
    node->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    parent->add_child(node);
    return node;
}
Button *button(Node *parent, const String &title) {
    auto *node = memnew(Button);
    node->set_text(title);
    node->set_custom_minimum_size(Vector2(0, 42));
    node->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
    parent->add_child(node);
    return node;
}
String absolute(const String &path) { return ProjectSettings::get_singleton()->globalize_path(path); }
bool write_file(const String &path, const String &text) {
    auto file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) return false;
    file->store_string(text);
    file->flush();
    return file->get_error() == OK;
}
}

void ProjectCreatorView::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    App::debug->set_enabled(false);
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    get_window()->set_min_size(Vector2i(900, 640));
    Ref<Theme> theme;
    theme.instantiate();
    theme->set_default_font_size(15);
    theme->set_color("font_color", "Label", INK);
    for (const char *type : {"Button", "LineEdit", "TextEdit", "CheckBox"}) {
        theme->set_color("font_color", type, INK);
        theme->set_color("font_hover_color", type, INK);
        theme->set_color("font_pressed_color", type, INK);
        theme->set_color("font_hover_pressed_color", type, INK);
        theme->set_color("font_focus_color", type, INK);
        theme->set_color("font_disabled_color", type, Color("6c7086"));
    }
    theme->set_stylebox("normal", "Button", box(Color("313244"), Color("45475a")));
    theme->set_stylebox("hover", "Button", box(Color("45475a"), Color("585b70")));
    theme->set_stylebox("pressed", "Button", box(Color("313244"), ACCENT));
    theme->set_stylebox("disabled", "Button", box(Color("181825"), Color("313244")));
    theme->set_stylebox("focus", "Button", box(Color(0, 0, 0, 0), ACCENT));
    // Keep checkbox content margins identical across every draw mode.
    for (const char *state : {"normal", "hover", "pressed", "hover_pressed", "disabled"}) {
        bool selected = std::string(state) == "pressed" || std::string(state) == "hover_pressed";
        theme->set_stylebox(state, "CheckBox", box(Color("313244"), selected ? ACCENT : Color("45475a"), 10, 14));
    }
    theme->set_stylebox("focus", "CheckBox", box(Color(0, 0, 0, 0), ACCENT, 10, 14));
    theme->set_stylebox("normal", "LineEdit", box(Color("181825"), Color("45475a"), 8));
    theme->set_stylebox("read_only", "LineEdit", box(Color("181825"), Color("313244"), 8));
    theme->set_stylebox("focus", "LineEdit", box(Color(0, 0, 0, 0), ACCENT, 8));
    theme->set_color("caret_color", "LineEdit", ACCENT);
    theme->set_stylebox("panel", "PopupMenu", box(Color("1e1e2e"), Color("45475a"), 10));
    theme->set_stylebox("hover", "PopupMenu", box(Color("45475a"), Color("45475a"), 6, 8));
    theme->set_color("font_color", "PopupMenu", INK);
    theme->set_color("font_hover_color", "PopupMenu", INK);
    theme->set_constant("v_separation", "PopupMenu", 14);
    theme->set_color("selection_color", "LineEdit", Color("585b70"));
    theme->set_stylebox("normal", "TextEdit", box(Color("11111b"), Color("45475a"), 10));
    theme->set_stylebox("read_only", "TextEdit", box(Color("11111b"), Color("45475a"), 10));
    theme->set_color("font_readonly_color", "TextEdit", Color("bac2de"));
    theme->set_color("font_placeholder_color", "TextEdit", Color("7f849c"));
    theme->set_constant("line_spacing", "TextEdit", 5);
    set_theme(theme);

    auto *background = memnew(ColorRect);
    background->set_color(Color("181825"));
    background->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(background);
    background->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    auto *layout = row(this, 0);
    layout->set_name("CreatorLayout");
    layout->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    auto *sidebar_panel = memnew(PanelContainer);
    sidebar = sidebar_panel;
    sidebar->set_name("Sidebar");
    sidebar->set_custom_minimum_size(Vector2(220, 0));
    sidebar_panel->add_theme_stylebox_override("panel", box(Color("11111b"), Color("313244"), 0, 22));
    layout->add_child(sidebar);
    auto *rail = column(sidebar, 16);
    label(rail, "M /", 34, ACCENT);
    label(rail, "MEOWODY", 19);
    label(rail, "PROJECT STUDIO", 10, MUTED);
    auto *rail_gap = memnew(Control);
    rail_gap->set_custom_minimum_size(Vector2(0, 20));
    rail->add_child(rail_gap);
    auto *nav = memnew(PanelContainer);
    nav->add_theme_stylebox_override("panel", box(Color("313244"), Color("45475a"), 9, 12));
    rail->add_child(nav);
    label(nav, String::utf8("＋  新建项目"), 15, ACCENT);
    label(rail, String::utf8("从想法，到第一个场景。"), 11, MUTED);
    auto *spacer = memnew(Control);
    spacer->set_v_size_flags(SIZE_EXPAND_FILL);
    rail->add_child(spacer);
    label(rail, "GODOT  /  C++", 11, MUTED);
    label(rail, String::utf8("轻量起步，自由构建。"), 11, MUTED);

    auto *scroll = memnew(ScrollContainer);
    content_scroll = scroll;
    scroll->set_name("ContentScroll");
    scroll->set_v_size_flags(SIZE_EXPAND_FILL);
    scroll->set_h_size_flags(SIZE_EXPAND_FILL);
    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    layout->add_child(scroll);
    auto *margin = memnew(MarginContainer);
    content_margin = margin;
    margin->set_h_size_flags(SIZE_EXPAND_FILL);
    margin->set_v_size_flags(SIZE_EXPAND_FILL);
    for (const char *side : {"margin_left", "margin_right", "margin_top", "margin_bottom"})
        margin->add_theme_constant_override(side, 28);
    scroll->add_child(margin);
    auto *body = column(margin, 24);
    auto *header = row(body);
    auto *heading = column(header, 7);
    label(heading, "WORKSPACE / NEW PROJECT", 11, ACCENT);
    label(heading, String::utf8("开启下一个项目"), 30);
    label(heading, String::utf8("准备好框架与引擎，专注创造你的作品。"), 14, MUTED)->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    auto *badge = memnew(PanelContainer);
    platform_badge = badge;
    badge->set_v_size_flags(SIZE_SHRINK_BEGIN);
    badge->add_theme_stylebox_override("panel", box(Color("313244"), Color("45475a"), 20, 10));
    header->add_child(badge);
    label(badge, String::utf8("WINDOWS  ·  x64"), 11, MUTED);

    const auto saved = App::save->load("creator", "settings");
    const auto settings = saved.ok() ? saved.data : Dictionary();
    saved_version = settings.get("version", "");
    columns = memnew(BoxContainer);
    columns->set_name("ContentColumns");
    columns->set_h_size_flags(SIZE_EXPAND_FILL);
    columns->set_v_size_flags(SIZE_EXPAND_FILL);
    columns->add_theme_constant_override("separation", 20);
    body->add_child(columns);
    auto *form = column(columns, 18);
    auto *project = card(form);
    label(project, String::utf8("01   项目信息"), 18);
    label(project, String::utf8("为新作品命名，选择它的起点。"), 13, MUTED);
    auto *project_row = memnew(BoxContainer);
    project_fields = project_row;
    project_row->set_name("ProjectFields");
    project_row->add_theme_constant_override("separation", 12);
    project->add_child(project_row);
    name = field(column(project_row, 7), "ProjectName", String::utf8("项目名称"), settings.get("name", "MyGame"));
    auto *ref_field = column(project_row, 7);
    auto *ref_heading = row(ref_field, 8);
    auto *ref_label = label(ref_heading, String::utf8("框架版本 / 分支"), 13, MUTED);
    ref_label->set_h_size_flags(SIZE_EXPAND_FILL);
    auto *refresh_refs = button(ref_heading, String::utf8("刷新"));
    refresh_refs->set_name("RefreshFrameworkRefs");
    refresh_refs->add_theme_stylebox_override("normal", box(Color("313244"), Color("45475a"), 6, 2));
    refresh_refs->add_theme_stylebox_override("hover", box(Color("45475a"), Color("585b70"), 6, 2));
    refresh_refs->add_theme_stylebox_override("pressed", box(Color("313244"), ACCENT, 6, 2));
    refresh_refs->add_theme_stylebox_override("disabled", box(Color("181825"), Color("313244"), 6, 2));
    refresh_refs->set_custom_minimum_size(Vector2(48, 0));
    refresh_refs->add_theme_font_size_override("font_size", 12);
    refresh_refs->connect("pressed", Callable(this, "run_operation").bind("refs"));
    actions.push_back(refresh_refs);
    ref = memnew(OptionButton);
    ref->set_name("FrameworkRef");
    ref->set_fit_to_longest_item(false);
    ref->set_clip_text(true);
    ref->set_custom_minimum_size(Vector2(0, 44));
    String saved_ref = settings.get("ref", "main");
    ref->add_item(saved_ref);
    ref->set_item_metadata(0, saved_ref);
    ref_field->add_child(ref);
    destination = field(project, "Destination", String::utf8("项目位置"), settings.get("destination", ""));
    destination->set_placeholder(String::utf8("选择父目录，或输入一个空目录的路径"));
    auto *directory_row = row(project);
    auto *directory_button = button(directory_row, String::utf8("选择文件夹…"));
    directory_button->connect("pressed", Callable(this, "browse").bind(false));
    actions.push_back(directory_button);
    auto *directory_hint = label(directory_row, String::utf8("现有文件不会被覆盖"), 11, MUTED);
    directory_hint->set_v_size_flags(SIZE_SHRINK_CENTER);

    auto *engine_card = card(form);
    label(engine_card, String::utf8("02   引擎设置"), 18);
    local = memnew(CheckBox);
    local->set_name("UseLocalEngine");
    local->set_text(String::utf8("使用本地 Godot 引擎"));
    local->set_pressed(settings.get("local", true));
    engine_card->add_child(local);
    engine_fields = column(engine_card, 10);
    label(engine_fields, String::utf8("可用的本地引擎"), 13, MUTED);
    engines = memnew(OptionButton);
    engines->set_name("LocalEngines");
    engines->set_fit_to_longest_item(false);
    engines->set_clip_text(true);
    engines->set_custom_minimum_size(Vector2(0, 44));
    engines->add_item(String::utf8("正在搜索兼容的 Godot 引擎…"));
    engine_fields->add_child(engines);
    engines->connect("item_selected", Callable(this, "engine_chosen"));
    engine = field(engine_fields, "EnginePath", String::utf8("已选引擎路径"), settings.get("engine", ""));
    engine->set_editable(false);
    engine->set_placeholder(String::utf8("从上方列表选择，或浏览引擎文件"));
    auto *engine_row = row(engine_fields);
    auto *rescan = button(engine_row, String::utf8("重新搜索"));
    rescan->set_name("discover");
    rescan->connect("pressed", Callable(this, "run_operation").bind("discover"));
    actions.push_back(rescan);
    auto *engine_button = button(engine_row, String::utf8("浏览引擎文件…"));
    engine_button->connect("pressed", Callable(this, "browse").bind(true));
    actions.push_back(engine_button);
    version_fields = column(engine_card, 10);
    label(version_fields, String::utf8("官方稳定版本（自动查询最新版）"), 13, MUTED);
    versions = memnew(OptionButton);
    versions->set_name("EngineVersion");
    versions->set_fit_to_longest_item(false);
    versions->set_custom_minimum_size(Vector2(0, 44));
    versions->set_text(String::utf8("等待查询官方版本…"));
    version_fields->add_child(versions);
    auto *versions_button = button(version_fields, String::utf8("刷新官方版本"));
    versions_button->set_name("versions");
    versions_button->connect("pressed", Callable(this, "run_operation").bind("versions"));
    actions.push_back(versions_button);
    label(engine_card, String::utf8("关闭本地引擎选项，将下载并校验官方稳定版。"), 11, MUTED)->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    local->connect("toggled", Callable(this, "mode_changed"));
    mode_changed(local->is_pressed());

    auto *primary_row = row(form);
    auto *create = button(primary_row, String::utf8("创建项目  →"));
    create->set_name("create");
    create->set_h_size_flags(SIZE_EXPAND_FILL);
    create->set_custom_minimum_size(Vector2(0, 50));
    create->add_theme_color_override("font_color", Color("1e1e2e"));
    create->add_theme_color_override("font_hover_color", Color("1e1e2e"));
    create->add_theme_color_override("font_pressed_color", Color("1e1e2e"));
    create->add_theme_stylebox_override("normal", box(ACCENT, ACCENT));
    create->add_theme_stylebox_override("hover", box(Color("b4befe"), Color("b4befe")));
    create->add_theme_stylebox_override("pressed", box(Color("b4befe"), Color("b4befe")));
    create->connect("pressed", Callable(this, "run_operation").bind("create"));
    actions.push_back(create);
    auto *rebuild = button(primary_row, String::utf8("重新编译"));
    rebuild->set_name("rebuild");
    rebuild->connect("pressed", Callable(this, "run_operation").bind("rebuild"));
    actions.push_back(rebuild);
    label(form, String::utf8("创建后自动编译与验证，成功后即可打开项目。"), 12, MUTED)->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);

    auto *workbench_card = memnew(PanelContainer);
    workbench_panel = workbench_card;
    workbench_panel->set_name("WorkbenchPanel");
    workbench_panel->set_custom_minimum_size(Vector2(460, 0));
    workbench_panel->set_v_size_flags(SIZE_EXPAND_FILL);
    workbench_card->add_theme_stylebox_override("panel", box(Color("1e1e2e"), Color("45475a"), 16, 20));
    columns->add_child(workbench_panel);
    auto *workbench = column(workbench_panel, 14);
    label(workbench, String::utf8("构建工作台"), 18);
    label(workbench, "LIVE OUTPUT", 10, MUTED);
    status = label(workbench, saved.ok() || saved.used_default ? String::utf8("就绪，等待创建") : String::utf8("偏好读取失败，已使用默认值。"), 14, ACCENT);
    status->set_name("Status");
    status->set_custom_minimum_size(Vector2(255, 44));
    status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    progress = memnew(ProgressBar);
    progress->set_show_percentage(false);
    progress->set_custom_minimum_size(Vector2(0, 4));
    progress->add_theme_stylebox_override("background", box(Color("45475a"), Color("45475a"), 2, 0));
    progress->add_theme_stylebox_override("fill", box(ACCENT, ACCENT, 2, 0));
    workbench->add_child(progress);
    label(workbench, String::utf8("准备依赖  /  编译扩展  /  验证运行"), 11, MUTED);
    log = memnew(TextEdit);
    log->set_name("BuildLog");
    log->set_editable(false);
    log->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
    log->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    log->set_placeholder(String::utf8("任务日志将在这里显示。\n\n你可以先检查工具链，\n确认一切准备就绪。"));
    log->set_custom_minimum_size(Vector2(0, 260));
    log->set_v_size_flags(SIZE_EXPAND_FILL);
    log->add_theme_font_size_override("font_size", 12);
    workbench->add_child(log);
    auto *tools = row(workbench, 8);
    auto *preflight = button(tools, String::utf8("检查工具链"));
    preflight->set_name("preflight");
    preflight->set_h_size_flags(SIZE_EXPAND_FILL);
    preflight->connect("pressed", Callable(this, "run_operation").bind("preflight"));
    actions.push_back(preflight);
    auto *folder = button(tools, String::utf8("打开目录"));
    folder->connect("pressed", Callable(this, "open_folder"));
    actions.push_back(folder);
    open = button(workbench, String::utf8("打开 Godot 项目  ↗"));
    open->set_name("OpenProject");
    open->set_disabled(true);
    open->connect("pressed", Callable(this, "open_project"));
    auto *footer = row(body);
    label(footer, "MEOWODY FRAMEWORK", 10, MUTED);
    auto *footer_spacer = memnew(Control);
    footer_spacer->set_h_size_flags(SIZE_EXPAND_FILL);
    footer->add_child(footer_spacer);
    label(footer, String::utf8("独立工程 · 完整源码 · 自由创作"), 11, MUTED);
    picker_scrim = memnew(ColorRect);
    picker_scrim->set_name("PickerScrim");
    picker_scrim->set_color(Color(0.04f, 0.04f, 0.07f, 0.72f));
    picker_scrim->set_mouse_filter(MOUSE_FILTER_STOP);
    add_child(picker_scrim);
    picker_scrim->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    picker_scrim->hide();
    get_window()->set_embedding_subwindows(true);
    Ref<Theme> dialog_style;
    dialog_style.instantiate();
    dialog_style->merge_with(theme);
    auto border = box(Color("1e1e2e"), Color("585b70"), 16, 20);
    border->set_shadow_color(Color(0, 0, 0, 0.35f));
    border->set_shadow_size(20);
    dialog_style->set_stylebox("panel", "AcceptDialog", border);
    for (const char *type : {"ItemList", "Tree"}) {
        dialog_style->set_stylebox("panel", type, box(Color("181825"), Color("45475a"), 10, 10));
        dialog_style->set_stylebox("selected", type, box(Color("45475a"), ACCENT, 6, 4));
        dialog_style->set_stylebox("selected_focus", type, box(Color("45475a"), ACCENT, 6, 4));
        dialog_style->set_color("font_color", type, INK);
        dialog_style->set_color("font_selected_color", type, INK);
    }
    picker = memnew(FileDialog);
    picker->set_name("LocationPicker");
    picker->set_use_native_dialog(false);
    picker->set_flag(Window::FLAG_BORDERLESS, true);
    picker->set_display_mode(FileDialog::DISPLAY_LIST);
    picker->set_access(FileDialog::ACCESS_FILESYSTEM);
    picker->set_theme(dialog_style);
    picker->set_title("MEOWODY / PROJECT STUDIO");
    add_child(picker);
    picker->set_transient(true);
    picker->set_exclusive(true);
    auto *dialog_body = picker->get_vbox();
    dialog_body->add_theme_constant_override("separation", 14);
    auto *dialog_header = row(dialog_body);
    dialog_body->move_child(dialog_header, 0);
    picker_heading = label(dialog_header, String::utf8("选择项目位置"), 24, ACCENT);
    picker_heading->set_h_size_flags(SIZE_EXPAND_FILL);
    auto *close_picker = button(dialog_header, String::utf8("×"));
    close_picker->set_name("CloseLocationPicker");
    close_picker->set_custom_minimum_size(Vector2(42, 42));
    close_picker->set_tooltip_text(String::utf8("关闭"));
    close_picker->connect("pressed", Callable(picker, "hide"));
    picker_hint = label(dialog_body, String::utf8("选择父目录，将在其中创建项目文件夹。"), 13, MUTED);
    dialog_body->move_child(picker_hint, 1);
    picker->get_cancel_button()->set_text(String::utf8("取消"));
    auto *confirm_picker = picker->get_ok_button();
    confirm_picker->add_theme_stylebox_override("normal", box(ACCENT, ACCENT));
    confirm_picker->add_theme_stylebox_override("hover", box(Color("b4befe"), Color("b4befe")));
    confirm_picker->add_theme_stylebox_override("pressed", box(Color("b4befe"), Color("b4befe")));
    for (const char *state : {"font_color", "font_hover_color", "font_pressed_color"})
        confirm_picker->add_theme_color_override(state, Color("1e1e2e"));
    picker->connect("visibility_changed", Callable(this, "picker_visibility_changed"));
    picker->connect("file_selected", Callable(this, "selected"));
    picker->connect("dir_selected", Callable(this, "selected"));
    quit_connection = App::event->connect<ApplicationRequest>([this](const ApplicationRequest &request) {
        if (busy) {
            request.consume();
            status->set_text(String::utf8("任务正在运行，请等待下载或编译完成后关闭。"));
        }
    });
    connect("resized", Callable(this, "update_layout"));
    update_layout();
    call_deferred("run_operation", "discover");
}

void ProjectCreatorView::set_busy(bool value) {
    busy = value;
    progress->set_indeterminate(value);
    status->add_theme_color_override("font_color", value ? Color("f9e2af") : ACCENT);
    for (auto *action : actions) action->set_disabled(value);
    for (auto *input : {name, destination}) input->set_editable(!value);
    ref->set_disabled(value);
    engines->set_disabled(value);
    versions->set_disabled(value);
    local->set_disabled(value);
    open->set_disabled(value || created.is_empty());
}

void ProjectCreatorView::mode_changed(bool use_local) {
    engine_fields->set_visible(use_local);
    version_fields->set_visible(!use_local);
    if (!use_local && status && !busy && versions->get_item_count() == 0) run_operation("versions");
}

void ProjectCreatorView::engine_chosen(int index) {
    if (busy) return;
    engine->set_text(index > 0 ? String(engines->get_item_metadata(index)) : String());
    engine->set_tooltip_text(engine->get_text());
}

void ProjectCreatorView::browse(bool choose_engine) {
    if (busy) return;
    choosing_engine = choose_engine;
    picker->clear_filters();
    picker->set_file_mode(choose_engine ? FileDialog::FILE_MODE_OPEN_FILE : FileDialog::FILE_MODE_OPEN_DIR);
    if (choose_engine) picker->add_filter("*.exe", "Godot");
    picker_heading->set_text(choose_engine ? String::utf8("选择 Godot 引擎") : String::utf8("选择项目位置"));
    picker_hint->set_text(choose_engine ? String::utf8("选择官方稳定版 Windows x64 编辑器，选中后将验证兼容性。") : String::utf8("选择父目录，将在其中创建以项目名称命名的文件夹。"));
    picker->get_ok_button()->set_text(choose_engine ? String::utf8("使用此引擎") : String::utf8("选择此文件夹"));
    picker->popup_centered_clamped(Vector2i(940, 660), 0.85);
}

void ProjectCreatorView::update_layout() {
    if (!columns) return;
    float width = get_size().x;
    bool stacked = width < 1280;
    sidebar->set_visible(width >= 1500);
    platform_badge->set_visible(width >= 1100);
    columns->set_vertical(stacked);
    project_fields->set_vertical(width < 1040);
    workbench_panel->set_custom_minimum_size(Vector2(stacked ? 0 : std::clamp(width * 0.28f, 340.f, 460.f), 0));
    for (const char *side : {"margin_left", "margin_right", "margin_top", "margin_bottom"})
        content_margin->add_theme_constant_override(side, width < 1280 ? 20 : 28);
    if (picker && picker->is_visible()) {
        Vector2i available = get_window()->get_size();
        picker->set_size(Vector2i(std::min(940, available.x - 40), std::min(660, available.y - 40)));
        picker->set_position((available - picker->get_size()) / 2);
    }
}

void ProjectCreatorView::picker_visibility_changed() {
    picker_scrim->set_visible(picker->is_visible());
    if (picker->is_visible()) update_layout();
}

void ProjectCreatorView::selected(String path) {
    if (busy) return;
    if (choosing_engine) {
        previous_engine = engine->get_text();
        engine->set_text(path);
        local->set_pressed(true);
        run_operation("inspect");
    }
    else {
        String directory_name = name->get_text().strip_edges();
        if (!directory_name.is_valid_filename() || directory_name.ends_with(".") || directory_name.ends_with(" ")) {
            status->set_text(String::utf8("请输入合法的项目目录名称。"));
            return;
        }
        destination->set_text(path.path_join(directory_name));
    }
}

void ProjectCreatorView::run_operation(String requested) {
    if (busy) return;
    if (requested != "versions" && requested != "preflight" && requested != "create" && requested != "rebuild" && requested != "discover" && requested != "inspect" && requested != "refs") return;
    if ((requested == "create" || requested == "rebuild") && (destination->get_text().strip_edges().is_empty() ||
            name->get_text().strip_edges().is_empty() || (local->is_pressed() && engine->get_text().strip_edges().is_empty()))) {
        status->set_text(String::utf8("请填写项目名称、目标目录及引擎。"));
        return;
    }
    if (requested == "create" && !local->is_pressed() && versions->get_selected() < 0) {
        status->set_text(String::utf8("请先查询并选择一个官方稳定版本。"));
        return;
    }
    if (requested == "rebuild" && !local->is_pressed() && executable.is_empty()) {
        status->set_text(String::utf8("请先选择创建项目时使用的本地引擎。"));
        return;
    }
    Dictionary settings;
    settings["name"] = name->get_text().strip_edges();
    settings["destination"] = destination->get_text().strip_edges();
    settings["ref"] = ref->get_selected_metadata();
    settings["local"] = local->is_pressed();
    settings["engine"] = engine->get_text().strip_edges();
    settings["version"] = versions->get_selected() >= 0 ? String(versions->get_selected_metadata()) : saved_version;
    auto saved = App::save->save("creator", "settings", settings);
    if (!saved) { status->set_text(saved.message); return; }
    const String directory = absolute("user://project_creator");
    if (DirAccess::make_dir_recursive_absolute(directory) != OK) { status->set_text("Cannot create creator cache."); return; }
    log_path = directory.path_join(String::num_uint64(Time::get_singleton()->get_ticks_usec()) + String(".log"));
    if (!write_file(log_path, "")) { status->set_text("Cannot write task log."); return; }
    creator::Job job;
    job.operation = requested == "create" ? "prepare" : requested.utf8().get_data();
    job.cache = directory.utf8().get_data();
    job.engine = engine->get_text().utf8().get_data();
    job.local = local->is_pressed();
    job.version = String(settings["version"]).utf8().get_data();
    job.ref = String(ref->get_selected_metadata()).utf8().get_data();
    job.destination = destination->get_text().strip_edges().utf8().get_data();
    if (OS::get_singleton()->has_feature("editor")) job.current_engine = OS::get_singleton()->get_executable_path().utf8().get_data();
    if (requested == "rebuild") {
        String path = destination->get_text().path_join("meowody-project.json");
        Variant parsed;
        if (FileAccess::file_exists(path)) parsed = JSON::parse_string(FileAccess::get_file_as_string(path));
        if (parsed.get_type() != Variant::DICTIONARY) { status->set_text(String::utf8("请选择已有框架工程的目录。")); return; }
        Dictionary metadata = parsed;
        job.expected_version = String(metadata.get("godot_version", "")).utf8().get_data();
        job.pin = String(metadata.get("godot_cpp_commit", "")).utf8().get_data();
        if (!local->is_pressed()) job.engine = executable.utf8().get_data();
    }
    operation = requested;
    if (requested == "create" || requested == "rebuild") { created = ""; executable = ""; }
    log->clear();
    log_length = -1;
    set_busy(true);
    status->set_text(requested == "discover" ? String::utf8("正在搜索本地引擎…") : requested == "versions" ? String::utf8("正在查询官方最新稳定版…") : requested == "refs" ? String::utf8("正在查询框架分支与发布标签…") : String::utf8("正在执行，请等待…"));
    start_job(std::move(job));
}

Dictionary ProjectCreatorView::generate_project(Dictionary request) { return creator::generate_project(request); }

void ProjectCreatorView::start_job(creator::Job job) {
    active_job = job;
    std::string output = log_path.utf8().get_data();
    auto id = get_instance_id();
    task = App::task->submit([job, output](const TaskContext &) {
        auto reply = std::make_shared<creator::Reply>(creator::execute(job, output));
        return TaskResult{reply->ok ? TaskError::None : TaskError::Failed, std::static_pointer_cast<void>(reply), reply->message};
    }, [id](const TaskResult &result) {
        auto *view = Object::cast_to<ProjectCreatorView>(ObjectDB::get_instance(id));
        if (view && view->is_inside_tree()) view->finished(result);
    });
    if (!task.valid() || task.status() == TaskStatus::Rejected) {
        set_busy(false);
        status->set_text("Task queue is unavailable.");
    }
}

void ProjectCreatorView::_process(double) {
    if (!busy || !FileAccess::file_exists(log_path)) return;
    auto file = FileAccess::open(log_path, FileAccess::READ);
    if (file.is_null() || int64_t(file->get_length()) == log_length) return;
    log_length = file->get_length();
    // Keep the latest 64 KiB visible; complete build logs remain on disk.
    file->seek(log_length > 65536 ? log_length - 65536 : 0);
    log->set_text(file->get_buffer(65536).get_string_from_utf8());
    log->set_v_scroll(log->get_line_count());
}

void ProjectCreatorView::finished(const TaskResult &result) {
    _process(0);
    Dictionary response;
    auto value = std::get_if<std::shared_ptr<void>>(&result.value);
    auto reply = value ? std::static_pointer_cast<creator::Reply>(*value) : std::shared_ptr<creator::Reply>();
    if (reply) {
        response["ok"] = reply->ok;
        response["message"] = String::utf8(reply->message.c_str());
        Array available;
        for (const auto &item : reply->engines) {
            Dictionary entry;
            entry["path"] = String::utf8(item.path.c_str()); entry["version"] = String::utf8(item.version.c_str()); entry["api"] = String::utf8(item.api.c_str());
            available.push_back(entry);
        }
        response["engines"] = available;
        response["engine"] = String::utf8(reply->engine.path.c_str());
        response["destination"] = String::utf8(active_job.destination.c_str());
        if (reply->ok && operation == "versions") {
            Variant latest = JSON::parse_string(String::utf8(reply->text.c_str())), releases = JSON::parse_string(String::utf8(reply->extra.c_str()));
            std::vector<String> tags;
            if (latest.get_type() == Variant::DICTIONARY && releases.get_type() == Variant::ARRAY) {
                Array list = releases; list.push_back(latest);
                for (int i = 0; i < list.size(); ++i) {
                    if (list[i].get_type() != Variant::DICTIONARY) continue;
                    Dictionary release = list[i];
                    String tag = release.get("tag_name", "");
                    if (bool(release.get("draft", false)) || bool(release.get("prerelease", false)) || !std::regex_match(std::string(tag.utf8().get_data()), std::regex(R"(4\.\d+(\.\d+)?-stable)"))) continue;
                    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) tags.push_back(tag);
                }
            }
            std::sort(tags.begin(), tags.end(), [](const String &a, const String &b) { return a.naturalnocasecmp_to(b) > 0; });
            Array versions;
            for (const auto &tag : tags) versions.push_back(tag);
            response["versions"] = versions;
            if (tags.empty()) { response["ok"] = false; response["message"] = "Official stable version lookup returned no valid releases."; }
        }
        if (reply->ok && active_job.operation == "prepare") {
            String path = String::utf8(reply->source.c_str()).path_join("framework-dependencies.json");
            Variant parsed;
            if (FileAccess::file_exists(path)) parsed = JSON::parse_string(FileAccess::get_file_as_string(path));
            Dictionary metadata = parsed.get_type() == Variant::DICTIONARY ? Dictionary(parsed) : Dictionary();
            Variant record = metadata.get("godot_cpp", Variant());
            if (record.get_type() != Variant::DICTIONARY) {
                response["ok"] = false; response["message"] = "Framework revision lacks a valid dependency record.";
            } else {
                Dictionary dependency = record;
                creator::Job resolve = active_job;
                resolve.operation = "resolve"; resolve.source = reply->source; resolve.commit = reply->commit;
                resolve.engine = reply->engine.path; resolve.expected_version = reply->engine.version;
                resolve.framework_pin = String(dependency.get("commit", "")).utf8().get_data();
                resolve.legacy_dependency_record = !dependency.has("tag") && !dependency.has("api_versions");
                Variant supported = dependency.get("api_versions", Variant());
                if (supported.get_type() == Variant::ARRAY && Array(supported).has(String::utf8(reply->engine.api.c_str())))
                    resolve.cpp_tag = String(dependency.get("tag", "")).utf8().get_data();
                status->set_text(String::utf8("正在匹配所选引擎的 godot-cpp 版本…"));
                start_job(std::move(resolve));
                return;
            }
        }
        if (reply->ok && active_job.operation == "resolve") {
            Dictionary request;
            request["name"] = name->get_text().strip_edges(); request["source"] = String::utf8(active_job.source.c_str());
            request["destination"] = String::utf8(active_job.destination.c_str()); request["engine_version"] = String::utf8(active_job.expected_version.c_str());
            String api = String::utf8(active_job.expected_version.c_str());
            api = api.get_slice(".", 0) + String(".") + api.get_slice(".", 1);
            request["api_version"] = api; request["framework_commit"] = String::utf8(active_job.commit.c_str());
            request["godot_cpp_commit"] = String::utf8(reply->cpp_commit.c_str()); request["godot_cpp_tag"] = String::utf8(reply->cpp_tag.c_str());
            Dictionary generated = creator::generate_project(request);
            if (bool(generated.get("ok", false))) {
                creator::Job build = active_job;
                build.operation = "build"; build.pin = reply->cpp_commit; build.cpp_tag = reply->cpp_tag;
                status->set_text(String::utf8("工程已生成，正在准备匹配的依赖并编译…"));
                start_job(std::move(build));
                return;
            }
            response["ok"] = false; response["message"] = generated.get("message", "Project generation failed.");
        }
    }
    bool success = result.ok() && bool(response.get("ok", false));
    if (!success) status->set_text(String::utf8("未完成：") + String(response.get("message", String::utf8(result.message.c_str()))));
    else if (operation == "refs") {
        String selected = ref->get_selected_metadata();
        ref->clear();
        int selection = 0;
        for (const auto &item : reply->refs) {
            String target = String::utf8(item.ref.c_str()), title = String::utf8(item.name.c_str());
            int index = ref->get_item_count();
            ref->add_item((item.kind == "branch" ? String::utf8("分支  ·  ") : String::utf8("标签  ·  ")) + title);
            ref->set_item_metadata(index, target);
            ref->set_item_tooltip(index, target);
            if (selected == target || (item.kind == "branch" && selected == title)) selection = index;
        }
        ref->select(selection);
        status->set_text(String::utf8("框架分支与发布标签已更新，请从下拉框选择。"));
    }
    else if (operation == "versions") {
        Array available = response.get("versions", Array());
        String selected = versions->get_selected() >= 0 ? String(versions->get_selected_metadata()) : String();
        versions->clear();
        for (int i = 0; i < available.size(); ++i) {
            String tag = available[i];
            versions->add_item(tag + (i == 0 ? String::utf8("   ·   最新稳定版") : String()));
            versions->set_item_metadata(i, tag);
            if (tag == selected) versions->select(i);
        }
        status->set_text(String::utf8("官方稳定版本已更新，请从下拉框选择。"));
    } else if (operation == "discover" || operation == "inspect") {
        Array available = response.get("engines", Array());
        String chosen = engine->get_text().replace("\\", "/").simplify_path();
        if (operation == "discover") {
            engines->clear();
            engines->add_item(String::utf8("请选择一个已检测的引擎…"));
            engines->set_item_metadata(0, "");
        }
        int selected = 0;
        for (int i = 0; i < available.size(); ++i) {
            Dictionary item = available[i];
            String path = String(item["path"]).replace("\\", "/").simplify_path(), detected = item["version"];
            int index = -1;
            for (int j = 1; j < engines->get_item_count(); ++j)
                if (String(engines->get_item_metadata(j)).nocasecmp_to(path) == 0) index = j;
            if (index < 0) {
                index = engines->get_item_count();
                String title = String("Godot ") + detected.get_slice(".stable", 0) + String::utf8("  ·  x64  —  ") + path;
                engines->add_item(title);
                engines->set_item_metadata(index, path);
                engines->set_item_tooltip(index, path);
            }
            if (path.nocasecmp_to(chosen) == 0) selected = index;
        }
        engines->select(selected);
        engine->set_text(selected > 0 ? String(engines->get_item_metadata(selected)) : String());
        engine->set_tooltip_text(engine->get_text());
        status->set_text(available.is_empty() ? String::utf8("未找到兼容引擎，可浏览文件手动选择。") : String::utf8("兼容引擎已列出，请从下拉框选择。"));
    } else if (operation == "preflight") status->set_text(String::utf8("工具链检查通过。"));
    else {
        created = response["destination"];
        executable = response["engine"];
        status->set_text(String::utf8("完成：编译、插件导入、框架加载和空白场景运行均通过。"));
    }
    if (!success && operation == "inspect") engine->set_text(previous_engine);
    set_busy(false);
    if (operation == "refs") refs_loaded = true;
    if (operation == "discover" && !refs_loaded) call_deferred("run_operation", "refs");
    else if ((operation == "discover" || operation == "refs") && !local->is_pressed() && versions->get_item_count() == 0) call_deferred("run_operation", "versions");
    if (!success) status->add_theme_color_override("font_color", Color("f38ba8"));
}

void ProjectCreatorView::open_project() {
    if (busy || created.is_empty() || executable.is_empty()) return;
    PackedStringArray arguments;
    arguments.push_back("--editor");
    arguments.push_back("--path");
    arguments.push_back(created.path_join("project"));
    if (OS::get_singleton()->create_process(executable, arguments) < 0) status->set_text(String::utf8("无法启动 Godot，请检查引擎路径。"));
}

void ProjectCreatorView::open_folder() {
    if (busy) return;
    String path = created.is_empty() ? destination->get_text().strip_edges() : created;
    if (!DirAccess::dir_exists_absolute(path)) { status->set_text(String::utf8("目标目录尚不存在。")); return; }
    OS::get_singleton()->shell_open(path);
}
