#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>

#include "tween_internal.h"
namespace meowody::tween
{
	using namespace godot;
	using namespace detail;
	namespace
	{
		enum class Access
		{
			Global2,
			Local2,
			Scale2,
			Rotation2,
			Global3,
			Local3,
			Scale3,
			Rotation3,
			Alpha,
			Color,
			Size
		};
		Variant read(Object* obj, Access access)
		{
			auto* control = Object::cast_to<Control>(obj);
			auto* n2	  = Object::cast_to<Node2D>(obj);
			auto* n3	  = Object::cast_to<Node3D>(obj);
			auto* canvas  = Object::cast_to<CanvasItem>(obj);
			switch (access)
			{
				case Access::Global2:
					if (control) return control->get_global_position();
					if (n2) return n2->get_global_position();
					break;
				case Access::Local2:
					if (control) return control->get_position();
					if (n2) return n2->get_position();
					break;
				case Access::Scale2:
					if (control) return control->get_scale();
					if (n2) return n2->get_scale();
					break;
				case Access::Rotation2:
					if (control) return control->get_rotation();
					if (n2) return n2->get_rotation();
					break;
				case Access::Global3:
					if (n3) return n3->get_global_position();
					break;
				case Access::Local3:
					if (n3) return n3->get_position();
					break;
				case Access::Scale3:
					if (n3) return n3->get_scale();
					break;
				case Access::Rotation3:
					if (n3) return n3->get_rotation();
					break;
				case Access::Alpha:
					if (canvas) return canvas->get_modulate().a;
					break;
				case Access::Color:
					if (canvas) return canvas->get_modulate();
					break;
				case Access::Size:
					if (control) return control->get_size();
					break;
			}
			return {};
		}
		void write(Object* obj, Access access, const Variant& value)
		{
			auto* control = Object::cast_to<Control>(obj);
			auto* n2	  = Object::cast_to<Node2D>(obj);
			auto* n3	  = Object::cast_to<Node3D>(obj);
			auto* canvas  = Object::cast_to<CanvasItem>(obj);
			switch (access)
			{
				case Access::Global2:
					if (control)
						control->set_global_position(value);
					else if (n2)
						n2->set_global_position(value);
					break;
				case Access::Local2:
					if (control)
						control->set_position(value);
					else if (n2)
						n2->set_position(value);
					break;
				case Access::Scale2:
					if (control)
						control->set_scale(value);
					else if (n2)
						n2->set_scale(value);
					break;
				case Access::Rotation2:
					if (control)
						control->set_rotation(double(value));
					else if (n2)
						n2->set_rotation(double(value));
					break;
				case Access::Global3:
					if (n3) n3->set_global_position(value);
					break;
				case Access::Local3:
					if (n3) n3->set_position(value);
					break;
				case Access::Scale3:
					if (n3) n3->set_scale(value);
					break;
				case Access::Rotation3:
					if (n3) n3->set_rotation(value);
					break;
				case Access::Alpha:
					if (canvas)
					{
						auto color = canvas->get_modulate();
						color.a	   = double(value);
						canvas->set_modulate(color);
					}
					break;
				case Access::Color:
					if (canvas) canvas->set_modulate(value);
					break;
				case Access::Size:
					if (control) control->set_size(value);
					break;
			}
		}
		TweenHandle native(Node* obj, const Variant& end, double duration,
						   Access access, const char* key)
		{
			auto s = make(Kind::Property, duration);
			s->to  = end;
			s->key = key;
			if (!obj)
			{
				fail(*s, "Tween target is null");
				return TweenHandle(s);
			}
			s->target = obj->get_instance_id();
			auto id	  = s->target;
			s->getter = [id, access]() {
				auto* o = ObjectDB::get_instance(id);
				return o ? read(o, access) : Variant();
			};
			s->setter = [id, access](const Variant& v) {
				if (auto* o = ObjectDB::get_instance(id)) write(o, access, v);
			};
			return TweenHandle(s);
		}
	}  // namespace
	TweenHandle Tween::move(Node* o, const Vector2& v, double t)
	{
		return native(o, v, t, Access::Global2, "global_position");
	}
	TweenHandle Tween::move(Node* o, const Vector3& v, double t)
	{
		return native(o, v, t, Access::Global3, "global_position");
	}
	TweenHandle Tween::local_move(Node* o, const Vector2& v, double t)
	{
		return native(o, v, t, Access::Local2, "position");
	}
	TweenHandle Tween::local_move(Node* o, const Vector3& v, double t)
	{
		return native(o, v, t, Access::Local3, "position");
	}
	TweenHandle Tween::scale(Node* o, const Vector2& v, double t)
	{
		return native(o, v, t, Access::Scale2, "scale");
	}
	TweenHandle Tween::scale(Node* o, const Vector3& v, double t)
	{
		return native(o, v, t, Access::Scale3, "scale");
	}
	TweenHandle Tween::rotate(Node* o, double v, double t)
	{
		return native(o, v, t, Access::Rotation2, "rotation");
	}
	TweenHandle Tween::rotate(Node* o, const Vector3& v, double t)
	{
		return native(o, v, t, Access::Rotation3, "rotation");
	}
	TweenHandle Tween::rotate_degrees(Node* o, double v, double t)
	{
		return rotate(o, v * 0.017453292519943295, t);
	}
	TweenHandle Tween::rotate_degrees(Node* o, const Vector3& v, double t)
	{
		return rotate(o, v * 0.017453292519943295, t);
	}
	TweenHandle Tween::fade(Node* o, double v, double t)
	{
		return native(o, v, t, Access::Alpha, "modulate:a");
	}
	TweenHandle Tween::color(Node* o, const Color& v, double t)
	{
		return native(o, v, t, Access::Color, "modulate");
	}
	TweenHandle Tween::size(Node* o, const Vector2& v, double t)
	{
		return native(o, v, t, Access::Size, "size");
	}
	TweenHandle Tween::property(Object* obj, const NodePath& path,
								const Variant& end, double duration)
	{
		auto s = make(Kind::Property, duration);
		s->to  = end;
		s->key = String(path);
		if (!obj || path.is_empty())
		{
			fail(*s, "Property target/path missing");
			return TweenHandle(s);
		}
		s->target = obj->get_instance_id();
		auto id	  = s->target;
		s->getter = [id, path]() {
			auto* o = ObjectDB::get_instance(id);
			return o ? o->get_indexed(path) : Variant();
		};
		s->setter = [id, path](const Variant& v) {
			if (auto* o = ObjectDB::get_instance(id)) o->set_indexed(path, v);
		};
		return TweenHandle(s);
	}
}  // namespace meowody::tween
