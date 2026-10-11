#include "ui_center.h"
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <algorithm>
namespace meowody {
using namespace godot;
void UICenter::refresh_layers() {
    auto snapshot=instances;
    std::sort(snapshot.begin(),snapshot.end(),[](auto& a,auto& b) { auto x=a->config->options.kind,y=b->config->options.kind; return x!=y?x<y:a->order<b->order; });
    Instance *page=nullptr,*modal=nullptr;
    for(auto& i:snapshot) if(i->attached && !i->state->close_requested && !stopped) { if(i->config->options.kind==UIKind::Page) page=i.get(); if(i->config->options.kind==UIKind::Modal) modal=i.get(); }
    int index=100;
    for(auto& i:snapshot) {
        auto* layer=Object::cast_to<CanvasLayer>(ObjectDB::get_instance(i->layer));
        auto* root=Object::cast_to<Node>(ObjectDB::get_instance(i->state->root_id)); if(!layer||!root) continue;
        layer->set_layer(index++); bool hidden=i->config->options.kind==UIKind::Page && i.get()!=page && !i->state->close_requested;
        bool blocked=i->state->close_requested || (modal && i.get()!=modal && i->config->options.kind<=UIKind::Modal);
        layer->set_visible(!hidden);
        if(auto* c=Object::cast_to<Control>(root)) { c->set_mouse_behavior_recursive(blocked?Control::MOUSE_BEHAVIOR_DISABLED:Control::MOUSE_BEHAVIOR_INHERITED); c->set_focus_behavior_recursive(blocked?Control::FOCUS_BEHAVIOR_DISABLED:Control::FOCUS_BEHAVIOR_INHERITED); }
        if(hidden && i->state->status==UIStatus::Open) { i->state->status=UIStatus::Suspended; if(i->presenter) i->presenter->on_suspend(); else if(root->has_method("on_suspend")) root->call("on_suspend"); }
        else if(!hidden && i->state->status==UIStatus::Suspended) { i->state->status=UIStatus::Open; if(i->presenter) i->presenter->on_resume(); else if(root->has_method("on_resume")) root->call("on_resume"); }
    }
    auto* t=Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop()); if(!t||!t->get_root()) return;
    auto* modal_root=modal?Object::cast_to<Node>(ObjectDB::get_instance(modal->state->root_id)):nullptr;
    if(auto* focus=Object::cast_to<Control>(ObjectDB::get_instance(restore_focus)); focus&&focus->is_visible_in_tree()&&(!modal_root||modal_root->is_ancestor_of(focus)||modal_root==focus)) focus->grab_focus();
    restore_focus=0;
    auto* focus=t->get_root()->gui_get_focus_owner();
    if(modal_root&&(!focus||(!modal_root->is_ancestor_of(focus)&&focus!=modal_root))) {
        std::vector<Node*> pending{modal_root};
        while(!pending.empty()) { auto* n=pending.back(); pending.pop_back(); if(auto* c=Object::cast_to<Control>(n);c&&c->is_visible_in_tree()&&c->get_focus_mode()!=Control::FOCUS_NONE) { c->grab_focus(); break; } for(int j=n->get_child_count()-1;j>=0;--j) pending.push_back(n->get_child(j)); }
    }
}
}
