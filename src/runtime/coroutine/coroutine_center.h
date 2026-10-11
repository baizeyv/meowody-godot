//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_COROUTINE_MANAGER_H
#define GDMEOWODYFRAMEWORK_COROUTINE_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "runtime/coroutine/coroutine.h"

namespace meowody
{
	class CoroutineCenter
	{
	  public:
		using CoroutineId = std::uint64_t;

	  private:
		static constexpr std::uint32_t invalid_index() noexcept
		{
			return (std::numeric_limits<std::uint32_t>::max)();
		}

	  public:
		CoroutineCenter() = default;

		[[nodiscard]]
		CoroutineId start(std::shared_ptr<Coroutine> p_coroutine);

		bool stop(CoroutineId p_id) noexcept;

		void update(double p_delta_time);

		void clear() noexcept;

		[[nodiscard]]
		double delta_time() const noexcept;

		[[nodiscard]]
		bool empty() const noexcept;
		std::size_t active_count() const noexcept { return m_active_count; }

	  private:
		struct Slot
		{
			std::shared_ptr<Coroutine> coroutine;
			std::uint32_t			   generation = 1;
			std::uint32_t			   next_free  = invalid_index();
			std::uint64_t started_at = 0;
			bool					   active	  = false;
		};

		static CoroutineId make_id(std::uint32_t p_index,
								   std::uint32_t p_generation) noexcept;

		static std::uint32_t id_index(CoroutineId p_id) noexcept;

		static std::uint32_t id_generation(CoroutineId p_id) noexcept;

		std::uint32_t allocate_slot();

		void recycle_slot(std::uint32_t p_index) noexcept;

	  private:
		double			  m_delta_time = 0.0;
		bool m_updating = false;
		std::vector<Slot> m_slots;
		std::uint32_t	  m_free_head	 = invalid_index();
		std::size_t		  m_active_count = 0;
		std::uint64_t m_start_sequence = 0;
	};
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_COROUTINE_MANAGER_H
