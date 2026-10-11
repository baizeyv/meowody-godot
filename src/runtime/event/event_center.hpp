//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_EVENT_CENTER_HPP
#define GDMEOWODYFRAMEWORK_EVENT_CENTER_HPP
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "runtime/core/type_token.hpp"
#include "runtime/event/event_channel.hpp"

namespace meowody {
class EventCenter {
public:
	EventCenter() : lifetime_(std::make_shared<ConnectionLifetime>()) {}

	~EventCenter() = default;

	template <typename Event, typename F>
	Connection connect(F &&callback) {
		using EventType = std::remove_cvref_t<Event>;

		EventChannel<EventType> *channel =
				get_or_create_channel<EventType>();

		return channel->connect(std::forward<F>(callback));
	}

	template <typename Event, typename Object, typename Method>
	Connection connect(Object *object, Method method) {
		static_assert(std::is_member_function_pointer_v<Method>,
				"Method must be a member function pointer.");

		using EventType = std::remove_cvref_t<Event>;

		static_assert(
				std::is_invocable_v<Method, Object *, const EventType &> ||
						std::is_invocable_v<Method, Object *>,
				"Member function must be callable as void(const Event&) or "
				"void().");

		return connect<EventType>(
				make_member_callback<EventType>(object, method));
	}

	template <typename Event>
	void emit(const Event &event) {
		using EventType = std::remove_cvref_t<Event>;

		EventChannel<EventType> *channel = find_channel<EventType>();

		if (!channel) {
			return;
		}

		channel->emit(event);
	}

	template <typename Event>
	void emit() {
		using EventType = std::remove_cvref_t<Event>;

		EventChannel<EventType> *channel = find_channel<EventType>();

		if (!channel) {
			return;
		}

		channel->emit();
	}

	template <typename Event>
	void clear() noexcept {
		using EventType = std::remove_cvref_t<Event>;

		EventChannel<EventType> *channel = find_channel<EventType>();

		if (channel != nullptr) {
			channel->clear();
		}
	}

	void clear_all() noexcept {
		for (auto &[key, channel] : channels_) {
			channel->clear();
		}
	}

	template <typename Event>
	[[nodiscard]]
	bool has_channel() const {
		using EventType = std::remove_cvref_t<Event>;

		return channels_.contains(type_token_of<EventType>());
	}

private:
	template <typename Event, typename Object, typename Method>
	requires std::is_invocable_v<Method, Object *, const Event &> static auto make_member_callback(Object *object, Method method) {
		return Delegate<void(const Event &)>::bind(object, method);
	}

	template <typename Event, typename Object, typename Method>
	requires(!std::is_invocable_v<Method, Object *, const Event &> &&
			std::is_invocable_v<Method, Object *>) static auto make_member_callback(Object *object, Method method) {
		return Delegate<void(const Event &)>([object, method](const Event &) {
			std::invoke(method, object);
		});
	}

	template <typename Event>
	EventChannel<Event> *get_or_create_channel() {
		const auto key = type_token_of<Event>();

		auto it = channels_.find(key);

		if (it != channels_.end()) {
			return static_cast<EventChannel<Event> *>(it->second.get());
		}

		auto channel = std::make_unique<EventChannel<Event>>(lifetime_);

		EventChannel<Event> *ptr = channel.get();

		channels_.emplace(key, std::move(channel));

		return ptr;
	}

	template <typename Event>
	EventChannel<Event> *find_channel() {
		const auto key = type_token_of<Event>();

		auto it = channels_.find(key);

		if (it == channels_.end()) {
			return nullptr;
		}

		return static_cast<EventChannel<Event> *>(it->second.get());
	}

private:
	std::shared_ptr<ConnectionLifetime> lifetime_;

	std::unordered_map<TypeToken, std::unique_ptr<EventChannelBase>,
			TypeTokenHash>
			channels_;
};
} // namespace meowody

#endif // GDMEOWODYFRAMEWORK_EVENT_CENTER_HPP
