#include "yatzy_view.h"
#include "runtime/app.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/center_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable.hpp>

using namespace godot;

namespace {
constexpr Color INK(0.914f, 0.933f, 0.949f), MUTED(0.569f, 0.643f, 0.710f),
    SURFACE(0.110f, 0.173f, 0.231f), TEAL(0.216f, 0.788f, 0.627f), GOLD(0.937f, 0.722f, 0.353f);

Ref<StyleBoxFlat> box(Color color, int radius = 12) {
    Ref<StyleBoxFlat> style;
    style.instantiate();
    style->set_bg_color(color);
    style->set_corner_radius_all(radius);
    style->set_content_margin_all(12);
    return style;
}

Label *label(Node *parent, String text, int size = 18, Color color = INK) {
    auto *node = memnew(Label);
    node->set_text(text);
    node->add_theme_font_size_override("font_size", size);
    node->add_theme_color_override("font_color", color);
    node->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    parent->add_child(node);
    return node;
}

Button *button(Node *parent, String text, Vector2 size, Color color = SURFACE) {
    auto *node = memnew(Button);
    node->set_text(text);
    node->set_custom_minimum_size(size);
    node->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
    node->add_theme_font_size_override("font_size", 18);
    node->add_theme_color_override("font_color", INK);
    node->add_theme_color_override("font_hover_color", INK);
    node->add_theme_color_override("font_pressed_color", INK);
    node->add_theme_color_override("font_disabled_color", MUTED);
    node->add_theme_stylebox_override("normal", box(color));
    node->add_theme_stylebox_override("hover", box(color.lightened(0.12)));
    node->add_theme_stylebox_override("pressed", box(color.darkened(0.12)));
    node->add_theme_stylebox_override("disabled", box(Color("22313e")));
    auto focus = box(Color(0, 0, 0, 0));
    focus->set_border_width_all(2);
    focus->set_border_color(TEAL);
    node->add_theme_stylebox_override("focus", focus);
    parent->add_child(node);
    return node;
}

void expand(Control *control) { control->set_h_size_flags(Control::SIZE_EXPAND_FILL); }

VBoxContainer *card(Node *parent) {
    auto *panel = memnew(PanelContainer);
    panel->add_theme_stylebox_override("panel", box(Color("142432"), 16));
    expand(panel);
    parent->add_child(panel);
    auto *content = memnew(VBoxContainer);
    content->add_theme_constant_override("separation", 8);
    panel->add_child(content);
    return content;
}
}

void YatzyView::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;
    meowody::App::debug->set_enabled(false);
    game_.instantiate();
    set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    auto *background = memnew(ColorRect);
    background->set_color(Color("0c1925"));
    background->set_mouse_filter(MOUSE_FILTER_IGNORE);
    add_child(background);
    background->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

    auto *scroll = memnew(ScrollContainer);
    add_child(scroll);
    scroll->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
    scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
    auto *center = memnew(CenterContainer);
    expand(center);
    scroll->add_child(center);
    auto *margin = memnew(MarginContainer);
    for (const char *side : {"margin_left", "margin_right", "margin_top", "margin_bottom"}) {
        margin->add_theme_constant_override(side, 24);
    }
    center->add_child(margin);
    auto *body = memnew(VBoxContainer);
    body->set_custom_minimum_size(Vector2(800, 0));
    body->add_theme_constant_override("separation", 16);
    margin->add_child(body);

    auto *toolbar = memnew(HBoxContainer);
    toolbar->add_theme_constant_override("separation", 10);
    body->add_child(toolbar);
    button(toolbar, String::utf8("← 返回"), Vector2(100, 42))->connect("pressed", Callable(this, "on_back"));
    button(toolbar, String::utf8("设置"), Vector2(84, 42))->connect("pressed", Callable(this, "on_settings"));
    auto *spacer = memnew(Control);
    expand(spacer);
    toolbar->add_child(spacer);
    label(toolbar, String::utf8("道具  0"), 16, MUTED);

    auto *heading = memnew(HBoxContainer);
    body->add_child(heading);
    auto *title = memnew(VBoxContainer);
    expand(title);
    heading->add_child(title);
    label(title, "YAHTZEE", 36);
    round_ = label(title, String::utf8("单人模式  /  第 1 回合，共 13 回合"), 16, MUTED);
    auto *points = memnew(VBoxContainer);
    heading->add_child(points);
    label(points, String::utf8("总分"), 14, MUTED);
    total_ = label(points, "0", 36, TEAL);

    auto *columns = memnew(HBoxContainer);
    columns->add_theme_constant_override("separation", 16);
    body->add_child(columns);
    auto *upper = card(columns);
    auto *lower = card(columns);
    label(upper, String::utf8("上区  /  点数累计"), 17, MUTED);
    label(lower, String::utf8("下区  /  组合计分"), 17, MUTED);
    const char *names[] = {"一 · ONES", "二 · TWOS", "三 · THREES", "四 · FOURS", "五 · FIVES", "六 · SIXES",
        "三条", "四条", "葫芦", "小顺子", "大顺子", "五同 · YAHTZEE", "机会 · CHANCE"};
    for (int i = 0; i < 13; ++i) {
        rows_[i] = button(i < 6 ? upper : lower, "", Vector2(0, 43));
        rows_[i]->set_name(String("Category") + String::num_int64(i));
        rows_[i]->connect("pressed", Callable(this, "on_select").bind(i));
        auto *row = memnew(HBoxContainer);
        rows_[i]->add_child(row);
        row->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
        row->set_offset(SIDE_LEFT, 14);
        row->set_offset(SIDE_RIGHT, -14);
        row->set_mouse_filter(MOUSE_FILTER_IGNORE);
        expand(label(row, String::utf8(names[i]), 17));
        values_[i] = label(row, "—", 18, MUTED);
    }
    bonus_ = label(upper, "", 15, GOLD);
    bonus_->set_custom_minimum_size(Vector2(0, 43));
    extra_bonus_ = label(body, "", 14, MUTED);

    auto *dice_card = card(body);
    auto *dice_row = memnew(HBoxContainer);
    dice_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    dice_row->add_theme_constant_override("separation", 14);
    dice_card->add_child(dice_row);
    for (int i = 0; i < 5; ++i) {
        dice_[i] = button(dice_row, "—", Vector2(100, 88), Color("eef3f4"));
        dice_[i]->set_name(String("Die") + String::num_int64(i));
        dice_[i]->add_theme_font_size_override("font_size", 27);
        dice_[i]->add_theme_color_override("font_color", Color("142432"));
        dice_[i]->add_theme_color_override("font_hover_color", Color("142432"));
        dice_[i]->add_theme_color_override("font_pressed_color", Color("142432"));
        dice_[i]->connect("pressed", Callable(this, "on_hold").bind(i));
    }
    hint_ = label(dice_card, "", 16, MUTED);
    hint_->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);

    auto *actions = memnew(HBoxContainer);
    actions->add_theme_constant_override("separation", 16);
    body->add_child(actions);
    roll_ = button(actions, "", Vector2(0, 64), Color("188164"));
    roll_->set_name("Roll");
    expand(roll_);
    roll_->connect("pressed", Callable(this, "on_roll"));
    score_ = button(actions, "SCORE", Vector2(260, 64), Color("aa7226"));
    score_->set_name("Score");
    score_->connect("pressed", Callable(this, "on_score"));

    settings_ = memnew(AcceptDialog);
    settings_->set_title(String::utf8("设置与规则"));
    settings_->set_text(String::utf8("每回合最多掷 3 次，点击骰子保留或释放。\n选择一个空白计分项，再点击 SCORE 提交。\n蓝绿色数字是本次可得分，零分也可提交。\n上区达到 63 分奖励 35 分；13 项填满后结束。\n三条/四条：全部点数之和；葫芦：25 分。\n小顺子：30 分；大顺子：40 分；五同：50 分。\n重复五同：按 Joker 规则填格，原五同为 50 时奖励 100 分。\n当前版本没有道具。"));
    settings_->get_ok_button()->set_text(String::utf8("继续游戏"));
    settings_->add_button(String::utf8("重新开始"), false, "restart")->connect("pressed", Callable(this, "on_restart"));
    add_child(settings_);
    result_ = memnew(AcceptDialog);
    result_->set_title(String::utf8("本局完成"));
    result_->get_ok_button()->set_text(String::utf8("再来一局"));
    result_->connect("confirmed", Callable(this, "on_restart"));
    add_child(result_);
    exit_ = memnew(ConfirmationDialog);
    exit_->set_title(String::utf8("退出游戏"));
    exit_->set_text(String::utf8("退出本局？当前进度不会保存。"));
    exit_->get_ok_button()->set_text(String::utf8("退出"));
    exit_->get_cancel_button()->set_text(String::utf8("继续游戏"));
    exit_->connect("confirmed", Callable(this, "on_quit"));
    add_child(exit_);
    game_->connect("state_changed", Callable(this, "refresh"));
    refresh();
}

void YatzyView::refresh() {
    Dictionary state = game_->get_state();
    PackedInt32Array scores = state["scores"], previews = state["previews"], dice = state["dice"], held = state["held"];
    bool finished = state["finished"];
    int rolls_left = state["rolls_left"];
    int completed = state["rounds_completed"];
    total_->set_text(String::num_int64(state["total"]));
    round_->set_text(finished ? String::utf8("单人模式  /  13 回合已完成") :
        String::utf8("单人模式  /  第 ") + String::num_int64(completed + 1) + String::utf8(" 回合，共 13 回合"));
    bonus_->set_text(String::utf8("上区 ") + String::num_int64(state["upper_total"]) + String::utf8(" / 63   ·   ") +
        (int(state["upper_bonus"]) ? String::utf8("奖励 +35") : String::utf8("奖励待解锁 +35")));
    extra_bonus_->set_text(String::utf8("五同额外奖励  +") + String::num_int64(state["yahtzee_bonus"]) +
        String::utf8("     ·     选择计分项后点击 SCORE；每项仅能填写一次"));
    for (int i = 0; i < 13; ++i) {
        bool available = previews[i] >= 0;
        rows_[i]->set_disabled(!available);
        values_[i]->set_text(scores[i] >= 0 ? String::num_int64(scores[i]) : available ? "+" + String::num_int64(previews[i]) : "—");
        values_[i]->add_theme_color_override("font_color", scores[i] >= 0 ? INK : available ? TEAL : MUTED);
        rows_[i]->add_theme_stylebox_override("normal", box(selected_ == i ? Color("245c55") : SURFACE));
    }
    for (int i = 0; i < 5; ++i) {
        dice_[i]->set_disabled(dice[i] == 0 || finished);
        dice_[i]->set_text(dice[i] == 0 ? "—" : String::num_int64(dice[i]) + (held[i] ? String::utf8("\n已保留") : ""));
        dice_[i]->add_theme_stylebox_override("normal", box(held[i] ? TEAL : Color("eef3f4")));
    }
    roll_->set_text(String::utf8("ROLL  ·  ") + String::num_int64(rolls_left) + String::utf8(" 次剩余"));
    roll_->set_disabled(rolls_left == 0);
    score_->set_disabled(selected_ < 0 || previews[selected_] < 0);
    score_->set_text(selected_ >= 0 && previews[selected_] >= 0 ? "SCORE  +" + String::num_int64(previews[selected_]) : "SCORE");
    hint_->set_text(finished ? String::utf8("本局完成！可在设置中重新开始。") : dice[0] == 0 ?
        String::utf8("点击 ROLL 开始本回合") : rolls_left == 0 ? String::utf8("掷骰次数已用完，请选择计分项") :
        String::utf8("点击骰子保留，再掷一次；也可现在选择计分项"));
}

void YatzyView::on_roll() { game_->roll(); }
void YatzyView::on_hold(int index) {
    PackedInt32Array held = game_->get_state()["held"];
    game_->set_held(index, !held[index]);
}
void YatzyView::on_select(int category) {
    if (game_->preview_score(category) < 0) return;
    selected_ = category;
    refresh();
}
void YatzyView::on_score() {
    int category = selected_;
    selected_ = -1;
    if (!game_->score(category)) { refresh(); return; }
    Dictionary state = game_->get_state();
    if (bool(state["finished"])) {
        result_->set_text(String::utf8("13 个计分项全部完成！\n\n最终得分：") + String::num_int64(state["total"]) +
            String::utf8("\n上区奖励：") + String::num_int64(state["upper_bonus"]) +
            String::utf8("\n五同额外奖励：") + String::num_int64(state["yahtzee_bonus"]));
        result_->popup_centered(Vector2i(420, 230));
    }
}
void YatzyView::on_restart() {
    settings_->hide();
    result_->hide();
    selected_ = -1;
    game_->reset();
}
void YatzyView::on_settings() { settings_->popup_centered(Vector2i(580, 340)); }
void YatzyView::on_back() { exit_->popup_centered(Vector2i(380, 160)); }
void YatzyView::on_quit() { get_tree()->quit(); }
