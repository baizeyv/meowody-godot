#include <functional>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/method_tweener.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/callable_custom.hpp>

#include "tween_internal.h"
namespace meowody::tween::detail
{
	namespace
	{
		class ClockCall final : public godot::CallableCustom
		{
			std::weak_ptr<State> state;
			static bool equal(const CallableCustom* a, const CallableCustom* b)
			{
				return a == b;
			}
			static bool less(const CallableCustom* a, const CallableCustom* b)
			{
				return std::less<const void*>{}(a, b);
			}

		  public:
			explicit ClockCall(std::weak_ptr<State> s) : state(std::move(s)) {}
			uint32_t hash() const override
			{
				return uint32_t(reinterpret_cast<uintptr_t>(this));
			}
			godot::String get_as_text() const override
			{
				return "TweenCenter clock";
			}
			CompareEqualFunc get_compare_equal_func() const override
			{
				return equal;
			}
			CompareLessFunc get_compare_less_func() const override
			{
				return less;
			}
			godot::ObjectID get_object() const override { return {}; }
			bool is_valid() const override { return !state.expired(); }
			void call(const godot::Variant** args, int count, godot::Variant&,
					  GDExtensionCallError& error) const override
			{
				error.error = GDEXTENSION_CALL_OK;
				auto s		= state.lock();
				if (!s || count != 1) return;
				double time = *args[0];
				double delta =
					time >= s->driver_last ? time - s->driver_last : time;
				s->driver_last = time;
				advance_timeline(s, delta);
			}
		};
	}  // namespace
	void driver_stop(State& s)
	{
		if (s.driver.is_valid())
		{
			s.driver->kill();
			s.driver.unref();
		}
	}
	void driver_start(const std::shared_ptr<State>& s)
	{
		if (s->mode == UpdateMode::Manual || s->driver.is_valid()) return;
		auto* tree = godot::Object::cast_to<godot::SceneTree>(
			godot::Engine::get_singleton()->get_main_loop());
		if (!tree)
		{
			fail(*s, "SceneTree unavailable");
			return;
		}
		s->driver	   = tree->create_tween();
		s->driver_last = 0;
		if (s->owner)
			if (auto* owner = godot::Object::cast_to<godot::Node>(
					godot::ObjectDB::get_instance(s->owner)))
				s->driver->bind_node(owner);
		s->driver->set_ignore_time_scale(s->ignore_scale);
		s->driver->set_process_mode(s->mode == UpdateMode::Physics
										? godot::Tween::TWEEN_PROCESS_PHYSICS
										: godot::Tween::TWEEN_PROCESS_IDLE);
		s->driver
			->tween_method(godot::Callable(memnew(ClockCall(s))), 0.0, 1.0, 1.0)
			->set_trans(godot::Tween::TRANS_LINEAR);
		s->driver->set_loops();
	}
}  // namespace meowody::tween::detail
