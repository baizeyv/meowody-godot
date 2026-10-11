#include "ui_presenter.h"
#include <godot_cpp/variant/callable_custom.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/array.hpp>
#include <functional>
namespace meowody {
namespace {
struct SignalState { Delegate<void(const godot::Array&)> callback; bool active=true; };
class PresenterSignal final : public godot::CallableCustom {
    uint64_t source;
    std::shared_ptr<SignalState> state;
    static bool equal(const CallableCustom* a,const CallableCustom* b) { return a==b; }
    static bool less(const CallableCustom* a,const CallableCustom* b) { return std::less<const void*>{}(a,b); }
public:
    PresenterSignal(uint64_t id,std::shared_ptr<SignalState> value):source(id),state(std::move(value)) {}
    uint32_t hash() const override { return uint32_t(reinterpret_cast<uintptr_t>(this)); }
    godot::String get_as_text() const override { return "UI Presenter signal"; }
    CompareEqualFunc get_compare_equal_func() const override { return equal; }
    CompareLessFunc get_compare_less_func() const override { return less; }
    godot::ObjectID get_object() const override { return godot::ObjectID(source); }
    bool is_valid() const override { return state->active && godot::ObjectDB::get_instance(source); }
    void call(const godot::Variant** args,int count,godot::Variant&,GDExtensionCallError& error) const override {
        error.error=GDEXTENSION_CALL_OK; auto held=state;
        if(!is_valid()) return; godot::Array values; values.resize(count); for(int i=0;i<count;++i) values[i]=*args[i];
        held->callback(values);
    }
};
}
bool UIPresenter::connect(godot::Object* source,const godot::StringName& signal,Delegate<void(const godot::Array&)> callback) {
    if(disconnected || !source || !callback || !source->has_signal(signal)) return false;
    auto state=std::make_shared<SignalState>(); state->callback=std::move(callback); auto id=uint64_t(source->get_instance_id());
    godot::Callable callable(memnew(PresenterSignal(id,state)));
    if(source->connect(signal,callable)!=godot::OK) return false;
    defer([id,signal,callable,state] { state->active=false; auto* obj=godot::ObjectDB::get_instance(id); if(obj && obj->is_connected(signal,callable)) obj->disconnect(signal,callable); });
    return true;
}
}
