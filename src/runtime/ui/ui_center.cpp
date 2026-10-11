#include "ui_center.h"
#include "runtime/scene/scene_center.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>
namespace meowody {
using namespace godot;
namespace {
Node* node(uint64_t id) { return Object::cast_to<Node>(ObjectDB::get_instance(id)); }
SceneTree* tree() { return Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop()); }
void dispose(uint64_t id,bool immediate=false) { if(auto* n=node(id)) { if(immediate) memdelete(n); else n->queue_free(); } }
bool terminal(UIStatus s) { return s==UIStatus::Closed || s==UIStatus::Failed || s==UIStatus::Cancelled; }
}
UICenter::UICenter(ResourceCenter& r,SceneCenter* scenes):resources(r) {
    if(scenes) scene_connection=scenes->on_switched([this](const SceneResult&) { close_all(false); });
}
UICenter::~UICenter() { shutdown(); }
String UICenter::normalize(const String& path) const {
    auto result=path.replace("\\","/"); if(!result.contains("://")) result="res://"+result; return result.simplify_path();
}
bool UICenter::configure(const String& path,UIOptions options,UIPresenterFactory factory) {
    auto key=normalize(path); if(stopped || path.is_empty() || options.animation_timeout<=0 || !std::isfinite(options.animation_timeout)) return false;
    for(auto& i:instances) if(i->path==key) return false;
    for(auto it=cache.begin();it!=cache.end();) { if(it->path==key) { dispose(it->root); it=cache.erase(it); } else ++it; }
    auto config=std::make_shared<Config>(); config->options=options; config->factory=std::move(factory); configs[key]=std::move(config); return true;
}
UIHandle UICenter::open(const String& scene,const Variant& data) { return request(scene,data,false,{}); }
UIHandle UICenter::open_async(const String& scene,const Variant& data,UIOpenCallback callback) { return request(scene,data,true,std::move(callback)); }
UIHandle UICenter::request(const String& path,const Variant& data,bool asynchronous,UIOpenCallback callback) {
    auto key=normalize(path);
    if(stopped || path.is_empty()) {
        auto state=std::make_shared<detail::UIState>(); state->status=UIStatus::Failed; state->error="UICenter is closed or scene path is empty"; state->percent=100;
        if(!stopped && asynchronous && callback) {
            auto failed=std::make_shared<Instance>(); failed->state=state; state->id=next_id++; failed->config=std::make_shared<Config>(); failed->callbacks.push_back(std::move(callback)); instances.push_back(failed);
        }
        return UIHandle(state);
    }
    auto& config=configs[key]; if(!config) config=std::make_shared<Config>();
    if(!config->options.multiple) for(auto i:instances) {
        if(i->path!=key || terminal(i->state->status) || i->state->status==UIStatus::Exiting || i->state->close_requested) continue;
        i->data=data; i->data_pending=true; i->state->front_requested=true;
        if(callback) i->callbacks.push_back(std::move(callback));
        // Asynchronous entry points never execute user code on the caller's stack.
        if(!busy && !asynchronous) {
            busy=true;
            if(i->attached) deliver_data(*i);
            else { i->asynchronous=false; i->load.cancel(); start(*i); i->load={}; }
            i->order=next_order++; i->state->front_requested=false;
            if(terminal(i->state->status) && i->callbacks.empty()) { auto keep=i; retire(keep,false); }
            refresh_layers(); if(stopped) drain_shutdown(); busy=false;
        }
        return UIHandle(i->state);
    }
    auto i=std::make_shared<Instance>(); i->path=key; i->config=config; i->data=data; i->asynchronous=asynchronous;
    i->state=std::make_shared<detail::UIState>(); i->state->id=next_id++; if(callback) i->callbacks.push_back(std::move(callback));
    instances.push_back(i);
    if(!busy) {
        busy=true; start(*i);
        if(terminal(i->state->status)) retire(i,false);
        refresh_layers(); if(stopped) drain_shutdown(); busy=false;
    }
    return UIHandle(i->state);
}
void UICenter::start(Instance& i) {
    auto cached=std::find_if(cache.begin(),cache.end(),[&](auto& c) { return c.path==i.path; });
    if(cached!=cache.end()) { i.state->root_id=cached->root; cache.erase(cached); if(!node(i.state->root_id)) i.state->root_id=0; }
    if(i.asynchronous) {
        i.state->status=UIStatus::Loading;
        if(!i.state->root_id) i.load=i.config->options.pack_url.is_empty() ? resources.load_async<PackedScene>(i.path) : resources.load_pack_async<PackedScene>(i.config->options.pack_url,i.path);
        return;
    }
    if(!i.state->root_id) {
        if(!i.config->options.pack_url.is_empty()) { i.state->status=UIStatus::Failed; i.state->error="Remote packs require open_async"; return; }
        auto result=resources.load<PackedScene>(i.path);
        if(!result) { i.state->status=UIStatus::Failed; i.state->error=result.message; return; }
        if(auto* n=result.resource->instantiate()) i.state->root_id=n->get_instance_id();
    }
    if(!mount(i)) i.state->status=UIStatus::Failed;
}
bool UICenter::mount(Instance& i) {
    auto* root=node(i.state->root_id); auto* t=tree();
    if(!root || root->is_queued_for_deletion() || !t || !t->get_root()) { i.state->error="Cannot instantiate UI scene or SceneTree is unavailable"; return false; }
    auto* layer=memnew(CanvasLayer); layer->set_name("MeowodyUI"); layer->set_process_mode(Node::PROCESS_MODE_ALWAYS);
    i.layer=layer->get_instance_id(); t->get_root()->add_child(layer); i.order=next_order++;
    if(auto* focus=t->get_root()->gui_get_focus_owner()) i.focus=focus->get_instance_id();
    if(i.config->options.kind==UIKind::Modal) { auto* mask=memnew(ColorRect); layer->add_child(mask); mask->set_color(Color(0,0,0,0.45)); mask->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT); mask->set_mouse_filter(Control::MOUSE_FILTER_STOP); }
    root->set_process_mode(Node::PROCESS_MODE_INHERIT); layer->add_child(root);
    resolve_transition(i);
    if(i.config->factory) { i.presenter=i.config->factory(); if(!i.presenter) { i.state->error="Presenter factory returned null"; return false; } i.presenter->on_attach(*root); }
    i.attached=true; i.state->percent=100;
    if(auto* control=Object::cast_to<Control>(root)) control->show();
    deliver_data(i);
    if(!i.state->close_requested && !stopped) { if(!animate(i,true)) complete_open(i); }
    return true;
}
void UICenter::deliver_data(Instance& i) {
    i.data_pending=false;
    if(i.presenter) i.presenter->on_open(i.data);
    else if(auto* root=node(i.state->root_id); root && root->has_method("on_open")) root->call("on_open",i.data);
}
void UICenter::resolve_transition(Instance& i) {
    auto* root=node(i.state->root_id);
    auto& options=i.config->options;
    AnimationPlayer* player=nullptr;
    StringName property=root->get_meta("meowody_ui_animation_property",StringName());
    if(options.animation_player.is_empty() && property!=StringName())
        player=Object::cast_to<AnimationPlayer>(static_cast<Object*>(root->get(property)));
    if(!player) {
        auto path=options.animation_player;
        if(path.is_empty()) path=root->get_meta("meowody_ui_animation_player",NodePath("AnimationPlayer"));
        player=Object::cast_to<AnimationPlayer>(root->get_node_or_null(path));
    }
    i.transition=player?uint64_t(player->get_instance_id()):0;
    i.enter_name=options.enter_animation;
    i.exit_name=options.exit_animation;
    if(i.enter_name==StringName()) i.enter_name=root->get_meta("meowody_ui_enter",String("ui_enter"));
    if(i.exit_name==StringName()) i.exit_name=root->get_meta("meowody_ui_exit",String("ui_exit"));
}
bool UICenter::animate(Instance& i,bool entering) {
    if(!node(i.state->root_id)) return false;
    if(auto* old=Object::cast_to<AnimationPlayer>(node(i.animation))) old->stop();
    auto* player=Object::cast_to<AnimationPlayer>(node(i.transition));
    auto name=entering?i.enter_name:i.exit_name;
    if(!player || name==StringName() || !player->has_animation(name)) return false;
    if(player->get_animation(name)->get_loop_mode()!=Animation::LOOP_NONE) { UtilityFunctions::push_warning("UI transition animation must not loop: ",name); return false; }
    i.animation=player->get_instance_id(); i.playing=name; i.elapsed=0;
    i.state->status=entering?UIStatus::Entering:UIStatus::Exiting; player->play(name); return true;
}
void UICenter::complete_open(Instance& i) {
    if(i.state->close_requested || stopped) return;
    i.state->status=UIStatus::Open;
    if(i.presenter) i.presenter->on_opened();
    else if(auto* root=node(i.state->root_id); root && root->has_method("on_opened")) root->call("on_opened");
}
void UICenter::begin_close(Instance& i) {
    if(i.state->status==UIStatus::Exiting || terminal(i.state->status)) return;
    if(!i.attached) { i.load.cancel(); i.state->status=UIStatus::Cancelled; return; }
    if(i.presenter) i.presenter->on_closing();
    else if(auto* root=node(i.state->root_id); root && root->has_method("on_closing")) root->call("on_closing");
    if(auto* c=Object::cast_to<Control>(node(i.state->root_id))) { c->set_mouse_behavior_recursive(Control::MOUSE_BEHAVIOR_DISABLED); c->set_focus_behavior_recursive(Control::FOCUS_BEHAVIOR_DISABLED); }
    if(!animate(i,false)) i.state->status=UIStatus::Closed;
}
void UICenter::retire(const std::shared_ptr<Instance>& i,bool cache_allowed) {
    std::erase(instances,i); i->load.cancel();
    if(auto* player=Object::cast_to<AnimationPlayer>(node(i->animation))) player->stop();
    if(i->attached) {
        if(i->presenter) { i->presenter->on_close(); i->presenter->disconnect(); }
        else if(auto* root=node(i->state->root_id); root && root->has_method("on_close")) root->call("on_close");
    }
    auto root_id=i->state->root_id; i->state->root_id=0;
    if(auto* root=node(root_id)) {
        if(auto* c=Object::cast_to<Control>(root)) c->hide(); root->set_process_mode(Node::PROCESS_MODE_DISABLED);
        if(root->get_parent()) root->get_parent()->remove_child(root);
        if(cache_allowed && !stopped && capacity && i->config->options.cache && !root->is_queued_for_deletion()) cache.push_back({i->path,root_id}); else dispose(root_id,stopped);
    }
    dispose(i->layer,stopped); i->layer=0; i->state->percent=100;
    if(i->focus) restore_focus=i->focus;
    // Presenter destruction is deferred past the current signal/lifecycle callback.
    retired.push_back(i); evict();
}
UIHandle UICenter::get(uint64_t id) const { for(auto& i:instances) if(i->state->id==id) return UIHandle(i->state); return {}; }
UIHandle UICenter::find(const String& scene) const { auto key=normalize(scene); for(auto it=instances.rbegin();it!=instances.rend();++it) if((*it)->path==key && !(*it)->state->close_requested) return UIHandle((*it)->state); return {}; }
bool UICenter::close(uint64_t id) {
    auto h=get(id); if(!h.valid()) return false; h.close();
    if(!busy) { busy=true; auto snapshot=instances; for(auto& i:snapshot) if(i->state->id==id) { begin_close(*i); if(terminal(i->state->status)) retire(i,i->state->status==UIStatus::Closed); } refresh_layers(); if(stopped) drain_shutdown(); busy=false; }
    return true;
}
bool UICenter::close(const String& path) { auto h=find(path); return h.valid() && close(h.id()); }
void UICenter::close_all(bool include_persistent) { auto snapshot=instances; for(auto& i:snapshot) if(include_persistent || !i->config->options.persistent) close(i->state->id); }
bool UICenter::back() {
    std::shared_ptr<Instance> target;
    for(auto& i:instances) {
        auto& o=i->config->options; if(i->state->close_requested || !i->attached || !o.dismiss_on_back || (o.kind!=UIKind::Page && o.kind!=UIKind::Modal)) continue;
        if(!target || o.kind>target->config->options.kind || (o.kind==target->config->options.kind && i->order>target->order)) target=i;
    }
    return target && close(target->state->id);
}
void UICenter::update(double delta) {
    if(stopped || busy) return; busy=true;
    auto snapshot=instances;
    retired.clear(); if(!std::isfinite(delta)||delta<0) delta=0;
    for(auto& i:snapshot) {
        if(stopped) break;
        auto& s=*i->state;
        if(s.close_requested) begin_close(*i);
        if(s.status==UIStatus::Queued && !s.close_requested) start(*i);
        else if(s.status==UIStatus::Loading && !s.close_requested) {
            s.percent=s.root_id?100:i->load.progress_percent();
            if(s.root_id || i->load.is_finished()) {
                if(!s.root_id) { auto result=i->load.result(); if(result) { if(auto* n=result.resource->instantiate()) s.root_id=n->get_instance_id(); } else s.error=result.message; }
                if(!s.root_id || !mount(*i)) s.status=UIStatus::Failed;
                i->load={};
            }
        }
        if(i->attached && !node(s.root_id)) { s.status=UIStatus::Failed; s.error="UI root was removed externally"; }
        if(i->data_pending && i->attached && !s.close_requested) deliver_data(*i);
        if(s.status==UIStatus::Entering || s.status==UIStatus::Exiting) {
            auto* player=Object::cast_to<AnimationPlayer>(node(i->animation)); i->elapsed+=delta;
            if(!player || !player->is_playing() || player->get_current_animation()!=i->playing || i->elapsed>=i->config->options.animation_timeout) {
                if(player && i->elapsed>=i->config->options.animation_timeout) { player->stop(); UtilityFunctions::push_warning("UI transition timed out: ",i->path); }
                if(s.status==UIStatus::Exiting) s.status=UIStatus::Closed; else complete_open(*i);
            }
        }
        if(s.front_requested) { i->order=next_order++; s.front_requested=false; }
        if(s.close_requested && s.status!=UIStatus::Exiting) begin_close(*i);
        if(terminal(s.status)) retire(i,s.status==UIStatus::Closed);
        if(s.status==UIStatus::Open || s.status==UIStatus::Suspended || s.status==UIStatus::Failed) {
            auto callbacks=std::move(i->callbacks); i->callbacks.clear();
            for(auto& cb:callbacks) if(cb && !stopped && !s.close_requested) cb(UIHandle(i->state));
        } else if(s.status==UIStatus::Cancelled || s.status==UIStatus::Closed) i->callbacks.clear();
    }
    if(!stopped) refresh_layers(); else drain_shutdown(); busy=false;
}
void UICenter::evict() { while(cache.size()>capacity) { auto item=cache.front(); cache.pop_front(); dispose(item.root,stopped); } }
void UICenter::clear_cache() { auto old=capacity; capacity=0; evict(); capacity=old; }
void UICenter::shutdown() { if(stopped) return; stopped=true; scene_connection.disconnect(); if(!busy) drain_shutdown(); }
void UICenter::drain_shutdown() {
    auto pending=instances;
    for(auto& i:pending) { i->state->status=i->attached?UIStatus::Closed:UIStatus::Cancelled; i->callbacks.clear(); retire(i,false); }
    retired.clear(); clear_cache(); configs.clear();
}
}
