#pragma once

#include <string>

namespace meowody {
enum class SystemError { None, InvalidState, InitializationFailed };

struct SystemStatus {
	SystemError error = SystemError::None;
	std::string message;
	bool ok() const { return error == SystemError::None; }
	explicit operator bool() const { return ok(); }
};

// Optional lifecycle for plain C++ business services; no Godot registration.
class System {
public:
	virtual ~System() = default;
	virtual SystemStatus initialize() { return {}; }
	// Also cleans partial initialization after an initialize() failure.
	virtual void shutdown() noexcept {}
};
} // namespace meowody
