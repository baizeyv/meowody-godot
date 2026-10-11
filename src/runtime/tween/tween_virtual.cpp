#include <cmath>

#include "tween_internal.h"
namespace meowody::tween
{
	using namespace detail;
	namespace
	{
		TweenHandle virtual_value(const godot::Variant& from,
								  const godot::Variant& to, double duration,
								  Delegate<void(const godot::Variant&)> setter)
		{
			auto s			 = make(Kind::Virtual, duration);
			s->from			 = from;
			s->to			 = to;
			s->explicit_from = true;
			s->setter		 = std::move(setter);
			return TweenHandle(s);
		}
	}  // namespace
	TweenHandle DOVirtual::float_value(double a, double b, double t,
									   Delegate<void(double)> fn)
	{
		return virtual_value(a, b, t,
							 [fn = std::move(fn)](const godot::Variant& v) {
								 if (fn) fn(double(v));
							 });
	}
	TweenHandle DOVirtual::int_value(int64_t a, int64_t b, double t,
									 Delegate<void(int64_t)> fn)
	{
		return virtual_value(double(a), double(b), t,
							 [fn = std::move(fn)](const godot::Variant& v) {
								 if (fn) fn(int64_t(std::llround(double(v))));
							 });
	}
	TweenHandle DOVirtual::vector2(godot::Vector2 a, godot::Vector2 b, double t,
								   Delegate<void(godot::Vector2)> fn)
	{
		return virtual_value(a, b, t,
							 [fn = std::move(fn)](const godot::Variant& v) {
								 if (fn) fn(v);
							 });
	}
	TweenHandle DOVirtual::vector3(godot::Vector3 a, godot::Vector3 b, double t,
								   Delegate<void(godot::Vector3)> fn)
	{
		return virtual_value(a, b, t,
							 [fn = std::move(fn)](const godot::Variant& v) {
								 if (fn) fn(v);
							 });
	}
	TweenHandle DOVirtual::color(godot::Color a, godot::Color b, double t,
								 Delegate<void(godot::Color)> fn)
	{
		return virtual_value(a, b, t,
							 [fn = std::move(fn)](const godot::Variant& v) {
								 if (fn) fn(v);
							 });
	}
	TweenHandle DOVirtual::delayed_call(double delay, Delegate<void()> fn,
										bool ignore)
	{
		auto h = callback(std::move(fn));
		h.delay(delay).ignore_time_scale(ignore);
		return h;
	}
	double DOVirtual::eased_value(double a, double b, double t, Ease e)
	{
		return double(interpolate(a, b, t, e));
	}
	int64_t DOVirtual::eased_value(int64_t a, int64_t b, double t, Ease e)
	{
		return int64_t(std::llround(eased_value(double(a), double(b), t, e)));
	}
	godot::Vector2 DOVirtual::eased_value(godot::Vector2 a, godot::Vector2 b,
										  double t, Ease e)
	{
		return interpolate(a, b, t, e);
	}
	godot::Vector3 DOVirtual::eased_value(godot::Vector3 a, godot::Vector3 b,
										  double t, Ease e)
	{
		return interpolate(a, b, t, e);
	}
	godot::Color DOVirtual::eased_value(godot::Color a, godot::Color b,
										double t, Ease e)
	{
		return interpolate(a, b, t, e);
	}
}  // namespace meowody::tween
