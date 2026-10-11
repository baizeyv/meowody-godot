//
// Created by baizeyv on 9/1/2026.
//

#include <cassert>

#include "coroutine_center.h"

namespace meowody
{
	CoroutineCenter::CoroutineId CoroutineCenter::start(
		std::shared_ptr<Coroutine> p_coroutine)
	{
		assert(p_coroutine != nullptr);

		const std::uint32_t index = allocate_slot();
		Slot& slot				  = m_slots[index];

		slot.coroutine = std::move(p_coroutine);
		slot.active	= true;
		slot.started_at = ++m_start_sequence;

		++m_active_count;

		return make_id(index, slot.generation);
	}

	bool CoroutineCenter::stop(const CoroutineId p_id) noexcept
	{
		const std::uint32_t index = id_index(p_id);

		if (index >= m_slots.size())
		{
			return false;
		}

		Slot& slot = m_slots[index];

		if (!slot.active || slot.generation != id_generation(p_id))
		{
			return false;
		}

		recycle_slot(index);
		return true;
	}

	void CoroutineCenter::update(const double p_delta_time)
	{
		if (m_updating) return;
		m_updating = true;
		struct Guard { bool& flag; ~Guard() { flag = false; } } guard{m_updating};
		m_delta_time = p_delta_time;

		if (m_active_count == 0)
		{
			return;
		}

		// Registration stamps defer new/replaced coroutines without a per-frame snapshot.
		const auto cutoff = m_start_sequence;
		const auto count = static_cast<std::uint32_t>(m_slots.size());
		for (std::uint32_t index = 0; index < count; ++index)
		{
			if (!m_slots[index].active || m_slots[index].started_at > cutoff) continue;
			const auto id = make_id(index, m_slots[index].generation);
			const auto coroutine = m_slots[index].coroutine;
			if (coroutine->move_next(p_delta_time)) stop(id);
		}
	}

	void CoroutineCenter::clear() noexcept
	{
		for (std::uint32_t index = 0; index < m_slots.size(); ++index)
		{
			if (m_slots[index].active)
			{
				recycle_slot(index);
			}
		}

		// Keep generations and free slots, so IDs issued before clear remain stale.
		m_active_count = 0;
	}

	double CoroutineCenter::delta_time() const noexcept
	{
		return m_delta_time;
	}

	bool CoroutineCenter::empty() const noexcept
	{
		return m_active_count == 0;
	}

	CoroutineCenter::CoroutineId CoroutineCenter::make_id(
		const std::uint32_t p_index, const std::uint32_t p_generation) noexcept
	{
		return (static_cast<CoroutineId>(p_generation) << 32u) | p_index;
	}

	std::uint32_t CoroutineCenter::id_index(const CoroutineId p_id) noexcept
	{
		return static_cast<std::uint32_t>(p_id & 0xffffffffu);
	}

	std::uint32_t CoroutineCenter::id_generation(
		const CoroutineId p_id) noexcept
	{
		return static_cast<std::uint32_t>(p_id >> 32u);
	}

	std::uint32_t CoroutineCenter::allocate_slot()
	{
		if (m_free_head != invalid_index())
		{
			const std::uint32_t index = m_free_head;
			Slot& slot				  = m_slots[index];

			m_free_head = slot.next_free;
			slot.next_free = invalid_index();

			return index;
		}

		m_slots.push_back({});
		return static_cast<std::uint32_t>(m_slots.size() - 1);
	}

	void CoroutineCenter::recycle_slot(const std::uint32_t p_index) noexcept
	{
		Slot& slot = m_slots[p_index];

		if (!slot.active)
		{
			return;
		}

		slot.active = false;
		slot.coroutine.reset();

		--m_active_count;
		++slot.generation;

		if (slot.generation == 0)
		{
			slot.generation = 1;
		}

		slot.next_free = m_free_head;
		m_free_head = p_index;
	}
}  // namespace meowody
