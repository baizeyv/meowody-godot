#pragma once
#include "ui_types.h"
#include "runtime/core/connection.hpp"
#include <vector>
namespace meowody {
// Optional, non-template MVP adapter. Native roots can instead expose on_* methods.
class UIPresenter {
    std::vector<Connection> connections;
    std::vector<Delegate<void()>> cleanup;
    bool disconnected=false;
public:
    virtual ~UIPresenter() { disconnect(); }
    void track(Connection connection) { if(!disconnected) connections.push_back(std::move(connection)); }
    void defer(Delegate<void()> action) { if(disconnected) { if(action) action(); } else cleanup.push_back(std::move(action)); }
    bool connect(godot::Object* source,const godot::StringName& signal,Delegate<void(const godot::Array&)> callback);
    void disconnect() {
        if(disconnected) return; disconnected=true;
        connections.clear(); auto actions=std::move(cleanup); cleanup.clear();
        for(auto it=actions.rbegin();it!=actions.rend();++it) if(*it) (*it)();
    }
    virtual void on_attach(godot::Node&) {}
    virtual void on_open(const godot::Variant&) {}
    virtual void on_opened() {}
    virtual void on_closing() {}
    virtual void on_close() {}
    virtual void on_suspend() {}
    virtual void on_resume() {}
};
using UIPresenterFactory = Delegate<std::unique_ptr<UIPresenter>()>;
}
