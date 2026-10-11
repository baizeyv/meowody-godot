//
// Created by baizeyv on 9/1/2026.
//

#include "wait_for_frames.h"

#include <algorithm>

namespace meowody
{
	WaitForFrames::WaitForFrames(const int p_frames) :
		m_remaining_frames((std::max)(0, p_frames)),
		m_frame_count((std::max)(0, p_frames))
	{
	}

	bool WaitForFrames::tick(double)
	{
		--m_remaining_frames;
		return m_remaining_frames <= 0;
	}

	void WaitForFrames::on_reset() noexcept
	{
		m_remaining_frames = m_frame_count;
	}

	WaitForFrames wait_for_frames(const int p_frames)
	{
		return WaitForFrames(p_frames);
	}
}  // namespace meowody