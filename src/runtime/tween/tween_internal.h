#pragma once
#include <godot_cpp/classes/tween.hpp>
#include <vector>

#include "tween_center.h"
namespace meowody::tween::detail
{
	enum class Kind
	{
		Interval,
		Property,
		Virtual,
		Callback,
		Sequence
	};
	enum class Op
	{
		Play,
		Pause,
		Restart,
		Rewind,
		Seek,
		Backwards,
		Complete,
		Kill
	};
	struct Command
	{
		Op	   op;
		double time	  = 0;
		bool   events = false, resume = false;
	};
	struct Child
	{
		double				   at;
		std::shared_ptr<State> value;
	};
	struct Track
	{
		State*		   leaf = nullptr;
		double		   at = 0, duration = 0;
		godot::Variant from, to;
		bool		   reverse = false, active = true;
	};
	struct Event
	{
		double at;
		State* leaf = nullptr;
		int	   hook = 0;
	};
	struct State
	{
		std::weak_ptr<Core>	 core;
		std::weak_ptr<State> parent;
		uint64_t			 id = 0, target = 0, owner = 0, revision = 0;
		godot::StringName	 tag;
		godot::String		 key, error;
		Kind				 kind	= Kind::Interval;
		Status				 status = Status::Pending;
		Ease				 easing = Ease::OutQuad;
		LoopType			 loop	= LoopType::Restart;
		UpdateMode			 mode	= UpdateMode::Idle;
		double span = 0, delay = 0, end = 0, last = 0, length = 0, time = 0,
			   speed = 1, driver_last = 0;
		int	   repeats = 1, direction = 1;
		bool   explicit_from = false, relative = false, ignore_scale = false,
			   compiled = false, started = false;
		bool   nested = false, busy = false, cancelled_tracks = false,
			   initial_events = true, claimed = false;
		godot::Variant						  from, to;
		Delegate<godot::Variant()>			  getter;
		Delegate<void(const godot::Variant&)> setter;
		Delegate<void()> action, start_cb, update_cb, step_cb, complete_cb,
			rewind_cb, kill_cb;
		std::vector<std::vector<size_t>> groups;
		std::vector<size_t>				 selected;
		std::vector<Child>				 children;
		std::vector<Track>				 tracks;
		std::vector<Event>				 events;
		std::vector<Command>			 commands;
		godot::Ref<godot::Tween>		 driver;
	};
	struct Core
	{
		bool								stopped = false, updating = false;
		uint64_t							next_id = 1;
		std::vector<std::shared_ptr<State>> active;
	};
	std::shared_ptr<State> make(Kind, double = 0);
	void				   fail(State&, const godot::String&);
	bool				   configurable(State&);
	double				   total(const State&);
	godot::Variant interpolate(const godot::Variant&, const godot::Variant&,
							   double, Ease);
	godot::Variant add(const godot::Variant&, const godot::Variant&);
	bool		   compile(State&);
	void		   evaluate(State&, double, bool force = false);
	void		   mark_children(State&, Status);
	void		   advance_timeline(const std::shared_ptr<State>&, double,
									bool events = true);
	void		   command(const std::shared_ptr<State>&, Command);
	void		   drain(const std::shared_ptr<State>&);
	void		   ensure_active(const std::shared_ptr<State>&);
	void		   claim(const std::shared_ptr<State>&);
	void		   driver_start(const std::shared_ptr<State>&);
	void		   driver_stop(State&);
	bool		   owners_valid(const State&);
}  // namespace meowody::tween::detail
