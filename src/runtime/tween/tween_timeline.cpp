#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

#include "tween_internal.h"
namespace meowody::tween::detail
{
	using namespace godot;
	Variant add(const Variant& a, const Variant& b)
	{
		Variant r;
		bool	valid = false;
		Variant::evaluate(Variant::OP_ADD, a, b, r, valid);
		return valid ? r : Variant();
	}
	Variant interpolate(const Variant& a, const Variant& b, double t, Ease ease)
	{
		if (t <= 0) return a;
		if (t >= 1) return b;
		Variant delta;
		bool	valid = false;
		Variant::evaluate(Variant::OP_SUBTRACT, b, a, delta, valid);
		if (!valid) return {};
		static const godot::Tween::TransitionType transitions[] = {
			godot::Tween::TRANS_SINE,	 godot::Tween::TRANS_QUAD,
			godot::Tween::TRANS_CUBIC,	 godot::Tween::TRANS_QUART,
			godot::Tween::TRANS_QUINT,	 godot::Tween::TRANS_EXPO,
			godot::Tween::TRANS_CIRC,	 godot::Tween::TRANS_BACK,
			godot::Tween::TRANS_ELASTIC, godot::Tween::TRANS_BOUNCE};
		int	 e = int(ease);
		auto trans =
			e == 0 ? godot::Tween::TRANS_LINEAR : transitions[(e - 1) / 3];
		auto mode = e == 0 ? godot::Tween::EASE_IN
						   : godot::Tween::EaseType((e - 1) % 3);
		return godot::Tween::interpolate_value(a, delta, t, 1, trans, mode);
	}
	namespace
	{
		bool alive(uint64_t id) { return !id || ObjectDB::get_instance(id); }
		bool flatten(State& root, const std::shared_ptr<State>& s,
					 double offset, bool reverse, bool is_root = false)
		{
			if (root.tracks.size() + root.events.size() > 100000)
			{
				fail(root, "Timeline exceeds 100000 compiled entries");
				return false;
			}
			double length =
				s->delay + (s->kind == Kind::Sequence ? s->end : s->span);
			if (s->status == Status::Failed || (!is_root && s->repeats < 0))
			{
				fail(root, "Invalid nested tween");
				return false;
			}
			int repeats = is_root ? 1 : s->repeats;
			if (!is_root && s->start_cb)
				root.events.push_back({offset, s.get(), 1});
			for (int n = 0; n < repeats; ++n)
			{
				if (root.tracks.size() + root.events.size() > 100000)
				{
					fail(root, "Timeline exceeds 100000 compiled entries");
					return false;
				}
				bool   rev	= reverse ^ (s->loop == LoopType::Yoyo && n % 2);
				double base = offset + (reverse ? repeats - 1 - n : n) * length;
				if (s->kind == Kind::Sequence)
				{
					for (auto& c : s->children)
					{
						double at = s->delay + c.at;
						if (!flatten(root, c.value,
									 base + (rev ? length - at - total(*c.value)
												 : at),
									 rev))
							return false;
					}
				}
				else if (s->kind == Kind::Callback)
					root.events.push_back(
						{base + (rev ? length - s->delay : s->delay), s.get(),
						 0});
				else if (s->kind == Kind::Property || s->kind == Kind::Virtual)
					root.tracks.push_back({s.get(),
										   base + (rev ? 0 : s->delay),
										   s->span,
										   {},
										   {},
										   rev,
										   true});
				if (!is_root && s->step_cb)
					root.events.push_back({base + length, s.get(), 4});
			}
			if (!is_root && s->complete_cb)
				root.events.push_back({offset + length * repeats, s.get(), 2});
			s->compiled = true;
			return true;
		}
		Variant track_value(const Track& t, double position)
		{
			double f =
				t.duration == 0
					? 1
					: std::clamp((position - t.at) / t.duration, 0.0, 1.0);
			return interpolate(t.from, t.to, t.reverse ? 1 - f : f,
							   t.leaf->easing);
		}
		double local_time(const State& s, double absolute)
		{
			if (s.length <= 0) return 0;
			auto   cycle = std::floor(absolute / s.length);
			double local = absolute - cycle * s.length;
			if (absolute > 0 && local == 0)
			{
				local = s.length;
				cycle -= 1;
			}
			if (s.loop == LoopType::Yoyo && std::fmod(cycle, 2.0) >= 1)
				local = s.length - local;
			return local;
		}
		bool writable_type(Variant::Type t)
		{
			return t == Variant::INT || t == Variant::FLOAT ||
				   t == Variant::VECTOR2 || t == Variant::VECTOR3 ||
				   t == Variant::COLOR;
		}
	}  // namespace
	bool owners_valid(const State& s)
	{
		if (!alive(s.owner)) return false;
		for (auto& c : s.children)
			if (!owners_valid(*c.value)) return false;
		return true;
	}
	bool compile(State& root)
	{
		if (root.compiled) return root.status != Status::Failed;
		root.length =
			root.delay + (root.kind == Kind::Sequence ? root.end : root.span);
		if (!std::isfinite(root.length) || root.length < 0 ||
			(root.length == 0 && root.repeats < 0) ||
			(root.repeats > 0 && !std::isfinite(total(root))))
		{
			fail(root, "Invalid or unbounded zero-duration timeline");
			return false;
		}
		auto owner = root.core.lock();
		if (!owner) return false;
		std::shared_ptr<State> ptr;
		for (auto& s : owner->active)
			if (s.get() == &root)
			{
				ptr = s;
				break;
			}
		if (!ptr)
		{
			fail(root, "Tween is not owned by a center");
			return false;
		}
		if (!flatten(root, ptr, 0, false, true)) return false;
		std::stable_sort(root.tracks.begin(), root.tracks.end(),
						 [](auto& a, auto& b) { return a.at < b.at; });
		std::map<State*, std::pair<Variant, Variant>> snapshots;
		for (size_t i = 0; i < root.tracks.size(); ++i)
		{
			auto& t	   = root.tracks[i];
			auto& leaf = *t.leaf;
			if (!alive(leaf.target))
			{
				t.active			  = false;
				root.cancelled_tracks = true;
				continue;
			}
			if (auto existing = snapshots.find(&leaf);
				existing != snapshots.end())
			{
				t.from = existing->second.first;
				t.to   = existing->second.second;
				continue;
			}
			Variant initial = leaf.explicit_from
								  ? leaf.from
								  : (leaf.getter ? leaf.getter() : leaf.from);
			if (!leaf.explicit_from && leaf.target)
				for (size_t j = 0; j < i; ++j)
				{
					auto& earlier = root.tracks[j];
					if (earlier.active && earlier.leaf->target == leaf.target &&
						earlier.leaf->key == leaf.key && earlier.at <= t.at)
						initial = track_value(earlier, t.at);
				}
			t.from = initial;
			t.to   = leaf.relative ? add(initial, leaf.to) : leaf.to;
			if (!writable_type(t.from.get_type()) ||
				!writable_type(t.to.get_type()) ||
				interpolate(t.from, t.to, 0.5, leaf.easing).get_type() ==
					Variant::NIL)
			{
				fail(root,
					 "Property/value type is missing or incompatible with "
					 "interpolation");
				return false;
			}
			snapshots[&leaf] = {t.from, t.to};
		}
		std::stable_sort(root.events.begin(), root.events.end(),
						 [](auto& a, auto& b) { return a.at < b.at; });
		std::map<std::pair<uint64_t, String>, size_t> property_groups;
		std::map<State*, size_t>					  virtual_groups;
		for (size_t n = 0; n < root.tracks.size(); ++n)
		{
			auto&  t = root.tracks[n];
			size_t index;
			if (t.leaf->target)
			{
				auto key = std::make_pair(t.leaf->target, t.leaf->key);
				auto [it, inserted] =
					property_groups.emplace(key, root.groups.size());
				if (inserted) root.groups.emplace_back();
				index = it->second;
			}
			else
			{
				auto [it, inserted] =
					virtual_groups.emplace(t.leaf, root.groups.size());
				if (inserted) root.groups.emplace_back();
				index = it->second;
			}
			root.groups[index].push_back(n);
		}
		root.selected.reserve(root.groups.size());
		return true;
	}
	void evaluate(State& s, double absolute, bool force)
	{
		double local	= local_time(s, absolute);
		auto   revision = s.revision;
		s.selected.clear();
		for (auto& group : s.groups)
		{
			size_t winner = s.tracks.size();
			for (auto n : group)
			{
				auto& t = s.tracks[n];
				if (!t.active) continue;
				if (!alive(t.leaf->target))
				{
					t.active		   = false;
					s.cancelled_tracks = true;
					continue;
				}
				if (winner == s.tracks.size() || t.at <= local) winner = n;
			}
			if (winner < s.tracks.size()) s.selected.push_back(winner);
		}
		std::sort(s.selected.begin(), s.selected.end());
		for (auto n : s.selected)
		{
			auto& t = s.tracks[n];
			if (!force && local < t.at && t.leaf->kind == Kind::Virtual &&
				s.direction > 0 && s.loop != LoopType::Yoyo)
				continue;
			if (t.leaf->setter) t.leaf->setter(track_value(t, local));
			if (s.revision != revision) return;
			if (t.leaf != &s && t.leaf->update_cb) t.leaf->update_cb();
			if (s.revision != revision) return;
		}
	}
	void mark_children(State& s, Status status)
	{
		for (auto& c : s.children)
		{
			c.value->status = status;
			mark_children(*c.value, status);
		}
	}
	void ensure_active(const std::shared_ptr<State>& s)
	{
		auto c = s->core.lock();
		if (!c || c->stopped) return;
		if (std::find(c->active.begin(), c->active.end(), s) == c->active.end())
			c->active.push_back(s);
	}
	namespace
	{
		bool conflict(const String& a, const String& b)
		{
			auto left  = a.get_slice(":", 0).trim_prefix("global_");
			auto right = b.get_slice(":", 0).trim_prefix("global_");
			return left == right;
		}
	}  // namespace
	void claim(const std::shared_ptr<State>& s)
	{
		if (s->claimed) return;
		s->claimed = true;
		auto c	   = s->core.lock();
		if (!c) return;
		auto snapshot = c->active;
		for (auto& old : snapshot)
			if (old != s && old->compiled && !old->nested &&
				(old->status == Status::Playing ||
				 old->status == Status::Paused))
			{
				for (auto& prior : old->tracks)
					if (prior.active && prior.leaf->target)
						for (auto& next : s->tracks)
							if (next.active &&
								next.leaf->target == prior.leaf->target &&
								conflict(next.leaf->key, prior.leaf->key))
							{
								prior.active		  = false;
								old->cancelled_tracks = true;
								break;
							}
				if (old->cancelled_tracks && old->events.empty() &&
					std::none_of(old->tracks.begin(), old->tracks.end(),
								 [](auto& t) { return t.active; }))
					command(old, {Op::Kill});
			}
	}
	namespace
	{
		struct Crossing
		{
			double time;
			State* leaf;
			int	   hook;
			size_t event_key;
		};
		void traverse(State& s, double from, double to, bool include_start)
		{
			if (s.events.empty() && !s.step_cb) return;
			bool   forward = to >= from;
			double lo = std::min(from, to), hi = std::max(from, to);
			std::vector<Crossing> crossed;
			double				  length = s.length;
			if (length == 0)
			{
				if (include_start)
				{
					if (double(s.repeats) * double(s.events.size() + 1) >
						100000)
					{
						fail(s, "Too many instantaneous events");
						return;
					}
					for (int cycle = 0; cycle < s.repeats; ++cycle)
					{
						size_t base = size_t(cycle) * (s.events.size() + 1);
						for (size_t index = 0; index < s.events.size(); ++index)
						{
							auto& e = s.events[index];
							crossed.push_back(
								{0, e.leaf, e.hook, base + index});
						}
						if (s.step_cb)
							crossed.push_back(
								{0, nullptr, 3, base + s.events.size()});
					}
				}
			}
			else
			{
				double first = std::max(0.0, std::floor(lo / length) - 1),
					   last	 = std::floor(hi / length);
				if (s.repeats > 0) last = std::min(last, double(s.repeats - 1));
				if (last - first > 100000)
				{
					fail(s, "One advance crosses too many loops");
					return;
				}
				for (double n = first; n <= last; ++n)
				{
					if (crossed.size() > 100000)
					{
						fail(s, "One advance crosses too many events");
						return;
					}
					bool reverse =
						s.loop == LoopType::Yoyo && std::fmod(n, 2.0) >= 1;
					for (size_t event_index = 0; event_index < s.events.size();
						 ++event_index)
					{
						auto&  e = s.events[event_index];
						double t =
							n * length + (reverse ? length - e.at : e.at);
						if ((forward ? (t > from && t <= to)
									 : (t >= to && t < from)) ||
							(include_start && t == from))
							crossed.push_back({t, e.leaf, e.hook, event_index});
					}
					double boundary = (forward ? n + 1 : n) * length;
					if ((forward ? (boundary > from && boundary <= to)
								 : (boundary >= to && boundary < from)))
						crossed.push_back({boundary, {}, 3, s.events.size()});
				}
			}
			std::stable_sort(
				crossed.begin(), crossed.end(), [&](auto& a, auto& b) {
					return forward ? a.time < b.time : a.time > b.time;
				});
			std::set<std::pair<double, size_t>> delivered;
			auto								rev = s.revision;
			for (auto& e : crossed)
			{
				if (e.leaf &&
					(e.leaf->kind == Kind::Property ||
					 e.leaf->kind == Kind::Virtual) &&
					std::none_of(
						s.tracks.begin(), s.tracks.end(),
						[&](auto& t) { return t.leaf == e.leaf && t.active; }))
					continue;
				if (!delivered.insert({e.time, e.event_key}).second) continue;
				s.time = e.time;
				evaluate(s, e.time);
				if (s.revision != rev) return;
				if (e.hook == 3)
				{
					if (s.step_cb) s.step_cb();
				}
				else
				{
					auto& fn =
						e.hook == 0
							? e.leaf->action
							: (e.hook == 1
								   ? e.leaf->start_cb
								   : (e.hook == 4 ? e.leaf->step_cb
												  : e.leaf->complete_cb));
					if (fn) fn();
				}
				if (s.revision != rev) return;
			}
		}
		void finish(State& s)
		{
			s.status =
				s.cancelled_tracks ? Status::Cancelled : Status::Completed;
			driver_stop(s);
			mark_children(s, s.status);
			if (s.cancelled_tracks)
			{
				if (s.kill_cb) s.kill_cb();
			}
			else if (s.complete_cb)
				s.complete_cb();
		}
	}  // namespace
	void advance_timeline(const std::shared_ptr<State>& ptr, double delta,
						  bool events)
	{
		auto& s = *ptr;
		if (s.busy || s.status != Status::Playing || !std::isfinite(delta) ||
			delta < 0)
			return;
		if (!owners_valid(s))
		{
			command(ptr, {Op::Kill});
			return;
		}
		s.busy	 = true;
		auto rev = s.revision;
		if (!s.started)
		{
			s.started = true;
			if (s.start_cb) s.start_cb();
		}
		if (s.revision == rev)
		{
			double dest = std::clamp(s.time + delta * s.speed * s.direction,
									 0.0, total(s));
			if (!std::isfinite(dest))
			{
				fail(s, "Playback time overflow");
				s.busy = false;
				drain(ptr);
				return;
			}
			if (events) traverse(s, s.time, dest, s.initial_events);
			s.initial_events = false;
			if (s.revision == rev)
			{
				s.time = dest;
				evaluate(s, dest);
			}
			if (s.revision == rev && s.update_cb) s.update_cb();
			if (s.revision == rev)
			{
				if (s.direction > 0 && dest >= total(s))
					finish(s);
				else if (s.direction < 0 && dest == 0)
				{
					s.status = Status::Paused;
					driver_stop(s);
					if (s.rewind_cb) s.rewind_cb();
				}
			}
		}
		s.busy = false;
		drain(ptr);
	}
	void command(const std::shared_ptr<State>& ptr, Command cmd)
	{
		if (!ptr) return;
		auto& s = *ptr;
		auto  c = s.core.lock();
		if (!c || c->stopped || s.status == Status::Cancelled ||
			s.status == Status::Failed)
			return;
		if (s.nested)
		{
			s.error = "Control the owning Sequence, not its child";
			return;
		}
		if (s.status == Status::Completed &&
			(cmd.op == Op::Complete || cmd.op == Op::Play))
			return;
		++s.revision;
		if (s.busy)
		{
			s.commands.push_back(cmd);
			return;
		}
		if (cmd.op == Op::Kill)
		{
			s.status = Status::Cancelled;
			mark_children(s, s.status);
			driver_stop(s);
			s.busy = true;
			if (s.kill_cb) s.kill_cb();
			s.busy = false;
			drain(ptr);
			return;
		}
		if (cmd.op == Op::Pause)
		{
			s.status = Status::Paused;
			driver_stop(s);
			return;
		}
		if (cmd.op == Op::Play && !s.compiled)
		{
			s.status = Status::Pending;
			ensure_active(ptr);
			return;
		}
		ensure_active(ptr);
		if (!compile(s)) return;
		s.busy	 = true;
		auto rev = s.revision;
		switch (cmd.op)
		{
			case Op::Play:
				s.status = Status::Playing;
				break;
			case Op::Restart:
				s.claimed		 = false;
				s.time			 = 0;
				s.direction		 = 1;
				s.started		 = false;
				s.initial_events = true;
				s.status		 = Status::Playing;
				evaluate(s, 0, true);
				break;
			case Op::Rewind: {
				bool moved		 = s.time != 0;
				s.time			 = 0;
				s.direction		 = 1;
				s.initial_events = false;
				s.status		 = Status::Paused;
				evaluate(s, 0, true);
				if (moved && s.revision == rev && s.rewind_cb) s.rewind_cb();
				break;
			}
			case Op::Backwards:
				if (s.status == Status::Completed) s.claimed = false;
				s.direction = -1;
				s.status	= Status::Playing;
				break;
			case Op::Seek:
			case Op::Complete: {
				double dest = cmd.op == Op::Complete ? total(s) : cmd.time;
				if (!std::isfinite(dest) || dest < 0)
				{
					s.error =
						"Seek/complete requires a finite nonnegative time";
					break;
				}
				dest = std::min(dest, total(s));
				if (cmd.events) traverse(s, s.time, dest, s.initial_events);
				s.initial_events = false;
				if (s.revision == rev)
				{
					s.time = dest;
					evaluate(s, dest, true);
				}
				if (s.revision == rev)
				{
					s.status = cmd.resume ? Status::Playing : Status::Paused;
					if (cmd.op == Op::Complete) finish(s);
				}
				break;
			}
			default:
				break;
		}
		s.busy = false;
		if (s.status == Status::Playing)
		{
			claim(ptr);
			if (s.status == Status::Playing) driver_start(ptr);
		}
		else
			driver_stop(s);
		drain(ptr);
	}
	void drain(const std::shared_ptr<State>& s)
	{
		if (s->busy) return;
		auto commands = std::move(s->commands);
		s->commands.clear();
		for (auto cmd : commands) command(s, cmd);
	}
}  // namespace meowody::tween::detail
