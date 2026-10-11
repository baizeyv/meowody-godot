#include "system_center.h"

namespace meowody {
SystemCenter::~SystemCenter() { clear(); }

SystemStatus SystemCenter::initialize() {
	if (state == State::Running) return {};
	if (state != State::Collecting)
		return {SystemError::InvalidState, "SystemCenter must be collecting registrations before initialize"};
	state = State::Initializing;
	for (auto key : registration_order) {
		auto &entry = entries.at(key);
		// Include the failing service in rollback to clean partial initialization.
		entry.started = true;
		if (!entry.lifecycle) continue;
		auto status = entry.lifecycle->initialize();
		if (!status.ok()) {
			state = State::ShuttingDown;
			stop_started();
			state = State::Collecting;
			return {SystemError::InitializationFailed,
				status.message.empty() ? "System initialization failed" : std::move(status.message)};
		}
	}
	state = State::Running;
	return {};
}

void SystemCenter::stop_started() noexcept {
	for (auto it = registration_order.rbegin(); it != registration_order.rend(); ++it) {
		auto &entry = entries.at(*it);
		if (!entry.started) continue;
		entry.started = false;
		if (entry.lifecycle) entry.lifecycle->shutdown();
	}
}

void SystemCenter::shutdown() noexcept {
	if (state == State::Initializing || state == State::ShuttingDown || state == State::Stopped) return;
	state = State::ShuttingDown;
	stop_started();
	state = State::Stopped;
}

void SystemCenter::clear() {
	if (state == State::Initializing || state == State::ShuttingDown) return;
	shutdown();
	state = State::ShuttingDown;
	while (!registration_order.empty()) {
		auto key = registration_order.back();
		registration_order.pop_back();
		auto entry = entries.extract(key);
		entry.mapped().instance.reset();
	}
	state = State::Collecting;
}
} // namespace meowody
