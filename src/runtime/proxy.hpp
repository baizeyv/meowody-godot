#pragma once

namespace meowody {
template <typename T>
class Proxy {
public:
	using Getter = T &(*)();

	constexpr explicit Proxy(Getter getter) : getter_(getter) {}

	T *operator->() const {
		return &getter_();
	}

private:
	Getter getter_;
};
}; //namespace meowody