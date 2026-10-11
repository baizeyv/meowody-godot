#pragma once
#include "runtime/core/connection.hpp"
#include "runtime/core/delegate.hpp"
#include <memory>
#include <type_traits>
#include <vector>
namespace meowody {
class EventChannelBase {
public:
	virtual ~EventChannelBase() = default;
	virtual void clear() noexcept = 0;
};
template <class Event>
class EventChannel final : public EventChannelBase {
	using Callback = Delegate<void(const Event &)>;
	struct Slot {
		std::shared_ptr<Callback> callback;
		uint32_t generation = 1;
		uint64_t connected_at = 0;
	};
	struct State {
		std::vector<Slot> slots;
		uint64_t connection_sequence = 0;
	};
	std::shared_ptr<State> state_ = std::make_shared<State>();
	std::shared_ptr<ConnectionLifetime> own_lifetime_ = std::make_shared<ConnectionLifetime>();
	static void disconnect_impl(void *owner, uint32_t index, uint32_t generation) noexcept {
		auto &slots = static_cast<EventChannel *>(owner)->state_->slots;
		if (index < slots.size() && slots[index].generation == generation) {
			slots[index].callback.reset();
			if (++slots[index].generation == 0)
				++slots[index].generation;
		}
	}

public:
	explicit EventChannel(std::weak_ptr<const ConnectionLifetime> = {}) {}
	~EventChannel() override { clear(); }
	EventChannel(const EventChannel &) = delete;
	EventChannel &operator=(const EventChannel &) = delete;
	bool empty() const noexcept {
		for (const auto &slot : state_->slots)
			if (slot.callback)
				return false;
		return true;
	}
	template <class F>
	Connection connect(F &&fn) {
		uint32_t i = 0;
		auto &slots = state_->slots;
		while (i < slots.size() && slots[i].callback)
			++i;
		if (i == slots.size())
			slots.emplace_back();
		if constexpr (std::is_invocable_v<F &, const Event &>)
			slots[i].callback = std::make_shared<Callback>(std::forward<F>(fn));
		else
			slots[i].callback = std::make_shared<Callback>([f = std::forward<F>(fn)](const Event &) mutable { f(); });
		slots[i].connected_at = ++state_->connection_sequence;
		return Connection(own_lifetime_, this, i, slots[i].generation, &disconnect_impl);
	}
	void emit(const Event &event) {
		// The immutable registration stamp excludes additions/reused slots from this emission.
		auto state = state_;
		const auto cutoff = state->connection_sequence;
		const auto count = state->slots.size();
		for (std::size_t i = 0; i < count; ++i) {
			auto &slot = state->slots[i];
			if (slot.connected_at > cutoff)
				continue;
			auto callback = slot.callback; // callable survives self-disconnect and vector growth
			if (callback)
				(*callback)(event);
		}
	}
	void emit() { emit(Event{}); }
	void clear() noexcept override {
		// Retain slot generations so old Connections cannot cancel new subscriptions.
		for (auto &slot : state_->slots) {
			slot.callback.reset();
			if (++slot.generation == 0)
				++slot.generation;
		}
	}
};
} //namespace meowody
