#pragma once

#include "system.h"
#include <memory>
#include <utility>
#include <unordered_map>
#include <vector>
#include <type_traits>

namespace meowody {
// Main-thread business services, registered once during game startup.
class SystemCenter {
	struct Entry {
		std::shared_ptr<void> instance;
		System *lifecycle = nullptr;
		bool started = false;
	};
	enum class State { Collecting, Initializing, Running, ShuttingDown, Stopped };
	std::unordered_map<const void *, Entry> entries;
	std::vector<const void *> registration_order;
	State state = State::Collecting;

	template <class T>
	static const void *type_key() {
		static const char key = 0;
		return &key;
	}

	void stop_started() noexcept;

public:
	SystemCenter() = default;
	SystemCenter(const SystemCenter &) = delete;
	SystemCenter &operator=(const SystemCenter &) = delete;
	~SystemCenter();

	template <class T>
	std::shared_ptr<T> get() const {
		auto found = entries.find(type_key<T>());
		return found == entries.end() ? std::shared_ptr<T>{} : std::static_pointer_cast<T>(found->second.instance);
	}

	template <class T, class... Args>
	std::shared_ptr<T> register_system(Args &&...args) {
		if (state == State::Initializing || state == State::ShuttingDown || state == State::Stopped)
			return {};
		if (auto existing = get<T>())
			return existing;
		if (state != State::Collecting)
			return {};
		auto instance = std::make_shared<T>(std::forward<Args>(args)...);
		Entry entry{instance};
		if constexpr (std::is_base_of_v<System, T>)
			entry.lifecycle = instance.get();
		entries.emplace(type_key<T>(), std::move(entry));
		registration_order.push_back(type_key<T>());
		return instance;
	}

	template <class... Types>
	bool register_systems() {
		return (static_cast<bool>(register_system<Types>()) && ...);
	}

	SystemStatus initialize();
	void shutdown() noexcept;
	void clear();
	bool is_initialized() const { return state == State::Running; }
};
} // namespace meowody
