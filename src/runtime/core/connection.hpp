//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_CONNECTION_HPP
#define GDMEOWODYFRAMEWORK_CONNECTION_HPP
#include <cstdint>
#include <memory>
#include <utility>

namespace meowody
{
	struct ConnectionLifetime
	{
	};

	class Connection
	{
	  public:
		Connection() noexcept = default;

		Connection(std::weak_ptr<const ConnectionLifetime> lifetime,
				   void* owner, std::uint32_t index, std::uint32_t generation,
				   void (*disconnect_fn)(void*, std::uint32_t,
										 std::uint32_t) noexcept) noexcept :
			lifetime_(std::move(lifetime)),
			tracks_lifetime_(true),
			owner_(owner),
			index_(index),
			generation_(generation),
			disconnect_fn_(disconnect_fn)
		{
		}

		Connection(void* owner, std::uint32_t index, std::uint32_t generation,
				   void (*disconnect_fn)(void*, std::uint32_t,
										 std::uint32_t) noexcept) noexcept :
			owner_(owner),
			index_(index),
			generation_(generation),
			disconnect_fn_(disconnect_fn)
		{
		}

		~Connection() { disconnect(); }

		Connection(Connection&& other) noexcept { move_from(std::move(other)); }

		Connection& operator=(Connection&& other) noexcept
		{
			if (this != &other)
			{
				disconnect();

				move_from(std::move(other));
			}

			return *this;
		}

		Connection(const Connection&) = delete;

		Connection& operator=(const Connection&) = delete;

		void disconnect() noexcept
		{
			if (!active())
			{
				return;
			}

			if (owner_alive())
			{
				disconnect_fn_(owner_, index_, generation_);
			}

			reset();
		}

		bool connected() const noexcept { return active() && owner_alive(); }

		explicit operator bool() const noexcept { return connected(); }

	  private:
		bool active() const noexcept
		{
			return owner_ != nullptr && disconnect_fn_ != nullptr;
		}

		bool owner_alive() const noexcept
		{
			return !tracks_lifetime_ || !lifetime_.expired();
		}

		void reset() noexcept
		{
			lifetime_.reset();
			tracks_lifetime_ = false;
			owner_			 = nullptr;
			disconnect_fn_	 = nullptr;
		}

		void move_from(Connection&& other) noexcept
		{
			lifetime_ = std::move(other.lifetime_);

			tracks_lifetime_ = other.tracks_lifetime_;

			owner_ = other.owner_;

			index_ = other.index_;

			generation_ = other.generation_;

			disconnect_fn_ = other.disconnect_fn_;

			other.reset();
		}

	  private:
		std::weak_ptr<const ConnectionLifetime> lifetime_;

		bool tracks_lifetime_ = false;

		void* owner_ = nullptr;

		std::uint32_t index_ = 0;

		std::uint32_t generation_ = 0;

		void (*disconnect_fn_)(void*, std::uint32_t,
							   std::uint32_t) noexcept = nullptr;
	};
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_CONNECTION_HPP
