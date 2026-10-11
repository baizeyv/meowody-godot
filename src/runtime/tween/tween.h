#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>

#include "runtime/core/delegate.hpp"
#include "runtime/coroutine/i_yield.h"
namespace meowody
{
	class TweenCenter;
}
namespace meowody::tween
{
	namespace detail
	{
		struct State;
		struct Core;
	}  // namespace detail
	enum class Status
	{
		Pending,
		Playing,
		Paused,
		Completed,
		Cancelled,
		Failed
	};
	enum class LoopType
	{
		Restart,
		Yoyo
	};
	enum class UpdateMode
	{
		Idle,
		Physics,
		Manual
	};
	enum class Ease
	{
		Linear,
		InSine,
		OutSine,
		InOutSine,
		InQuad,
		OutQuad,
		InOutQuad,
		InCubic,
		OutCubic,
		InOutCubic,
		InQuart,
		OutQuart,
		InOutQuart,
		InQuint,
		OutQuint,
		InOutQuint,
		InExpo,
		OutExpo,
		InOutExpo,
		InCirc,
		OutCirc,
		InOutCirc,
		InBack,
		OutBack,
		InOutBack,
		InElastic,
		OutElastic,
		InOutElastic,
		InBounce,
		OutBounce,
		InOutBounce
	};
	class TweenHandle
	{
	  protected:
		std::shared_ptr<detail::State> state;

	  public:
		TweenHandle() = default;
		explicit TweenHandle(std::shared_ptr<detail::State> value) :
			state(std::move(value))
		{
		}
		bool		  valid() const;
		Status		  status() const;
		godot::String error() const;
		uint64_t	  id() const;
		double		  duration() const;
		double		  position() const;
		double		  progress_percent() const;
		bool		  is_running() const;
		bool		  is_finished() const;
		TweenHandle&  ease(Ease value = Ease::OutQuad);
		TweenHandle&  delay(double seconds);
		TweenHandle&  from(const godot::Variant& value);
		TweenHandle&  relative(bool value = true);
		TweenHandle&  loops(int count, LoopType type = LoopType::Restart);
		TweenHandle&  speed_scale(double value);
		TweenHandle&  ignore_time_scale(bool value = true);
		TweenHandle&  process_mode(UpdateMode value);
		TweenHandle&  bind(godot::Node* owner);
		TweenHandle&  set_id(const godot::StringName& value);
		TweenHandle&  on_start(Delegate<void()> fn);
		TweenHandle&  on_update(Delegate<void()> fn);
		TweenHandle&  on_step_complete(Delegate<void()> fn);
		TweenHandle&  on_complete(Delegate<void()> fn);
		TweenHandle&  on_rewind(Delegate<void()> fn);
		TweenHandle&  on_kill(Delegate<void()> fn);
		TweenHandle&  play();
		TweenHandle&  pause();
		TweenHandle&  restart();
		TweenHandle&  rewind();
		TweenHandle&  seek(double time, bool events = false);
		TweenHandle&  goto_time(double time, bool and_play = true,
								bool events = false);
		TweenHandle&  play_backwards();
		TweenHandle&  complete(bool events = false);
		TweenHandle&  kill();
		// Deterministic/manual playback; only roots configured as Manual accept
		// this.
		void custom_step(double delta);
		friend class Sequence;
		friend class meowody::TweenCenter;
	};
	class Sequence : public TweenHandle
	{
	  public:
		explicit Sequence(std::shared_ptr<detail::State> value) :
			TweenHandle(std::move(value))
		{
		}
		static Sequence create(godot::Node* owner = nullptr);
		Sequence&		append(const TweenHandle&);
		Sequence&		join(const TweenHandle&);
		Sequence&		prepend(const TweenHandle&);
		Sequence&		insert(double seconds, const TweenHandle&);
		Sequence&		append_interval(double seconds);
		Sequence& operator<<(const TweenHandle& value) { return append(value); }
		Sequence& operator>>(const TweenHandle& value) { return join(value); }
	};
	struct Tween
	{
		static TweenHandle move(godot::Node*, const godot::Vector2&, double);
		static TweenHandle move(godot::Node*, const godot::Vector3&, double);
		static TweenHandle local_move(godot::Node*, const godot::Vector2&,
									  double);
		static TweenHandle local_move(godot::Node*, const godot::Vector3&,
									  double);
		static TweenHandle scale(godot::Node*, const godot::Vector2&, double);
		static TweenHandle scale(godot::Node*, const godot::Vector3&, double);
		static TweenHandle rotate(godot::Node*, double, double);
		static TweenHandle rotate(godot::Node*, const godot::Vector3&, double);
		static TweenHandle rotate_degrees(godot::Node*, double, double);
		static TweenHandle rotate_degrees(godot::Node*, const godot::Vector3&,
										  double);
		static TweenHandle fade(godot::Node*, double, double);
		static TweenHandle color(godot::Node*, const godot::Color&, double);
		static TweenHandle size(godot::Node*, const godot::Vector2&, double);
		static TweenHandle property(godot::Object*, const godot::NodePath&,
									const godot::Variant&, double);
	};
	TweenHandle callback(Delegate<void()> fn);
	TweenHandle interval(double seconds);
	struct DOVirtual
	{
		static TweenHandle float_value(double, double, double,
									   Delegate<void(double)>);
		static TweenHandle int_value(int64_t, int64_t, double,
									 Delegate<void(int64_t)>);
		static TweenHandle vector2(godot::Vector2, godot::Vector2, double,
								   Delegate<void(godot::Vector2)>);
		static TweenHandle vector3(godot::Vector3, godot::Vector3, double,
								   Delegate<void(godot::Vector3)>);
		static TweenHandle color(godot::Color, godot::Color, double,
								 Delegate<void(godot::Color)>);
		static TweenHandle delayed_call(double, Delegate<void()>,
										bool ignore_time_scale = true);
		static double eased_value(double, double, double, Ease = Ease::OutQuad);
		static int64_t		  eased_value(int64_t, int64_t, double,
										  Ease = Ease::OutQuad);
		static godot::Vector2 eased_value(godot::Vector2, godot::Vector2,
										  double, Ease = Ease::OutQuad);
		static godot::Vector3 eased_value(godot::Vector3, godot::Vector3,
										  double, Ease = Ease::OutQuad);
		static godot::Color	  eased_value(godot::Color, godot::Color, double,
										  Ease = Ease::OutQuad);
	};
	class WaitForTween final : public IYield
	{
		TweenHandle handle;

	  protected:
		bool tick(double) override { return handle.is_finished(); }

	  public:
		explicit WaitForTween(TweenHandle value) : handle(std::move(value)) {}
	};
	inline WaitForTween wait_for_tween(TweenHandle value)
	{
		return WaitForTween(std::move(value));
	}
}  // namespace meowody::tween
