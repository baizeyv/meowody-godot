#include <algorithm>
#include <cmath>
#include <limits>

#include "runtime/app.h"
#include "tween_internal.h"
namespace meowody::tween::detail
{
	std::shared_ptr<State> make(Kind kind, double duration)
	{
		auto s = App::tween->make();
		s->kind = kind;
		s->span = duration;
		if (!std::isfinite(duration) || duration < 0)
			fail(*s, "Duration must be finite and nonnegative");
		return s;
	}
	void fail(State& s, const godot::String& error)
	{
		s.error	 = error;
		s.status = Status::Failed;
		mark_children(s, s.status);
		++s.revision;
		driver_stop(s);
	}
	bool configurable(State& s)
	{
		if (s.compiled || s.nested)
		{
			s.error =
				"Configuration is locked after attachment or first playback";
			return false;
		}
		return s.status != Status::Failed && s.status != Status::Cancelled;
	}
	double total(const State& s)
	{
		return s.repeats < 0
				   ? std::numeric_limits<double>::infinity()
				   : (s.delay + (s.kind == Kind::Sequence ? s.end : s.span)) *
						 s.repeats;
	}
}  // namespace meowody::tween::detail
namespace meowody::tween
{
	using namespace detail;
	bool   TweenHandle::valid() const { return bool(state); }
	Status TweenHandle::status() const
	{
		if (!state) return Status::Failed;
		if (auto p = state->parent.lock()) return TweenHandle(p).status();
		return state->status;
	}
	godot::String TweenHandle::error() const
	{
		return state ? state->error : godot::String("Invalid TweenHandle");
	}
	uint64_t TweenHandle::id() const { return state ? state->id : 0; }
	double	 TweenHandle::duration() const { return state ? total(*state) : 0; }
	double	 TweenHandle::position() const { return state ? state->time : 0; }
	double	 TweenHandle::progress_percent() const
	{
		auto d = duration();
		return !std::isfinite(d)
				   ? -1
				   : (d > 0 ? std::clamp(position() / d * 100, 0.0, 100.0)
							: (is_finished() ? 100 : 0));
	}
	bool TweenHandle::is_running() const { return status() == Status::Playing; }
	bool TweenHandle::is_finished() const
	{
		auto s = status();
		return s == Status::Completed || s == Status::Cancelled ||
			   s == Status::Failed;
	}
	TweenHandle& TweenHandle::ease(Ease v)
	{
		if (state && configurable(*state)) state->easing = v;
		return *this;
	}
	TweenHandle& TweenHandle::delay(double v)
	{
		if (state && configurable(*state))
		{
			if (!std::isfinite(v) || v < 0)
				fail(*state, "Invalid delay");
			else
				state->delay = v;
		}
		return *this;
	}
	TweenHandle& TweenHandle::from(const godot::Variant& v)
	{
		if (state && configurable(*state))
		{
			state->from			 = v;
			state->explicit_from = true;
		}
		return *this;
	}
	TweenHandle& TweenHandle::relative(bool v)
	{
		if (state && configurable(*state)) state->relative = v;
		return *this;
	}
	TweenHandle& TweenHandle::loops(int n, LoopType t)
	{
		if (state && configurable(*state))
		{
			if (n == 0 || n < -1)
				fail(*state, "Loop count must be positive or -1");
			else
			{
				state->repeats = n;
				state->loop	   = t;
			}
		}
		return *this;
	}
	TweenHandle& TweenHandle::speed_scale(double v)
	{
		if (state)
		{
			if (!std::isfinite(v) || v <= 0)
			{
				state->error = "Speed must be finite and positive";
			}
			else
				state->speed = v;
		}
		return *this;
	}
	TweenHandle& TweenHandle::ignore_time_scale(bool v)
	{
		if (state && configurable(*state)) state->ignore_scale = v;
		return *this;
	}
	TweenHandle& TweenHandle::process_mode(UpdateMode v)
	{
		if (state && configurable(*state)) state->mode = v;
		return *this;
	}
	TweenHandle& TweenHandle::bind(godot::Node* v)
	{
		if (state && configurable(*state))
			state->owner = v ? uint64_t(v->get_instance_id()) : 0;
		return *this;
	}
	TweenHandle& TweenHandle::set_id(const godot::StringName& v)
	{
		if (state) state->tag = v;
		return *this;
	}
#define TWEEN_CALLBACK(method, field)                                    \
	TweenHandle& TweenHandle::method(Delegate<void()> fn)                \
	{                                                                    \
		if (state && configurable(*state)) state->field = std::move(fn); \
		return *this;                                                    \
	}
	TWEEN_CALLBACK(on_start, start_cb)
	TWEEN_CALLBACK(on_update, update_cb)
	TWEEN_CALLBACK(on_step_complete, step_cb)
	TWEEN_CALLBACK(on_complete, complete_cb)
	TWEEN_CALLBACK(on_rewind, rewind_cb)
	TWEEN_CALLBACK(on_kill, kill_cb)
#undef TWEEN_CALLBACK
	TweenHandle& TweenHandle::play()
	{
		command(state, {Op::Play});
		return *this;
	}
	TweenHandle& TweenHandle::pause()
	{
		command(state, {Op::Pause});
		return *this;
	}
	TweenHandle& TweenHandle::restart()
	{
		command(state, {Op::Restart});
		return *this;
	}
	TweenHandle& TweenHandle::rewind()
	{
		command(state, {Op::Rewind});
		return *this;
	}
	TweenHandle& TweenHandle::seek(double t, bool e)
	{
		command(state, {Op::Seek, t, e, false});
		return *this;
	}
	TweenHandle& TweenHandle::goto_time(double t, bool p, bool e)
	{
		command(state, {Op::Seek, t, e, p});
		return *this;
	}
	TweenHandle& TweenHandle::play_backwards()
	{
		command(state, {Op::Backwards});
		return *this;
	}
	TweenHandle& TweenHandle::complete(bool e)
	{
		command(state, {Op::Complete, 0, e});
		return *this;
	}
	TweenHandle& TweenHandle::kill()
	{
		command(state, {Op::Kill});
		return *this;
	}
	void TweenHandle::custom_step(double delta)
	{
		if (state && state->mode == UpdateMode::Manual &&
			state->status == Status::Playing)
			advance_timeline(state, delta);
	}
	Sequence Sequence::create(godot::Node* owner)
	{
		auto s	 = make(Kind::Sequence);
		s->owner = owner ? uint64_t(owner->get_instance_id()) : 0;
		return Sequence(s);
	}
	Sequence& Sequence::insert(double at, const TweenHandle& value)
	{
		if (!state || !value.state || !configurable(*state)) return *this;
		auto child = value.state;
		if (child == state || child->nested || child->compiled ||
			child->status == Status::Failed ||
			child->status == Status::Cancelled || child->repeats < 0 ||
			!std::isfinite(at) || at < 0)
		{
			state->error =
				"Invalid child, insertion time, or child already "
				"attached/started";
			return *this;
		}
		for (auto p = state; p; p = p->parent.lock())
			if (p == child)
			{
				state->error = "Sequence cycle rejected";
				return *this;
			}
		child->parent = state;
		child->nested = true;
		state->children.push_back({at, child});
		state->last = at;
		state->end	= std::max(state->end, at + total(*child));
		return *this;
	}
	Sequence& Sequence::append(const TweenHandle& v)
	{
		return insert(state ? state->end : 0, v);
	}
	Sequence& Sequence::join(const TweenHandle& v)
	{
		return insert(state ? state->last : 0, v);
	}
	Sequence& Sequence::prepend(const TweenHandle& v)
	{
		if (!state) return *this;
		auto old = state->children.size();
		insert(0, v);
		if (state->children.size() != old)
		{
			double d = total(*v.state);
			for (size_t i = 0; i < old; ++i) state->children[i].at += d;
			std::rotate(state->children.begin(), state->children.end() - 1,
						state->children.end());
			state->end = 0;
			for (auto& c : state->children)
				state->end = std::max(state->end, c.at + total(*c.value));
		}
		return *this;
	}
	Sequence& Sequence::append_interval(double t)
	{
		return append(interval(t));
	}
	TweenHandle callback(Delegate<void()> fn)
	{
		auto s	  = make(Kind::Callback);
		s->action = std::move(fn);
		return TweenHandle(s);
	}
	TweenHandle interval(double t)
	{
		return TweenHandle(make(Kind::Interval, t));
	}
}  // namespace meowody::tween
