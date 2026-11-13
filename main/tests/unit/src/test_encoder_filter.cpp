/*
 * This file is part of the ScrewSync project.
 * Copyright (c) 2026 Donatas Olsevičius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <zephyr/ztest.h>
#include "EncoderFilter.hpp"

using Encoder::Direction;
using Encoder::Filter;

namespace
{
    constexpr int32_t BACKLASH = 3;
    constexpr int64_t POLL_MS = 2;
}

ZTEST_SUITE(encoder_filter, NULL, NULL, NULL, NULL, NULL);

ZTEST(encoder_filter, test_first_reading_seeds)
{
    Filter f;
    zassert_equal(f.update(1234, 0, false, BACKLASH), 1234);
    zassert_equal(f.direction(0), Direction::Stopped);
}

ZTEST(encoder_filter, test_slop_inside_the_deadband_is_absorbed)
{
    Filter f;
    f.update(100, 0, false, BACKLASH);
    zassert_equal(f.update(103, POLL_MS, false, BACKLASH), 100);
    zassert_equal(f.update(97, 2 * POLL_MS, false, BACKLASH), 100);
    zassert_equal(f.direction(2 * POLL_MS), Direction::Stopped);
}

ZTEST(encoder_filter, test_follows_past_the_deadband)
{
    Filter f;
    f.update(100, 0, false, BACKLASH);
    zassert_equal(f.update(104, POLL_MS, false, BACKLASH), 101);
    zassert_equal(f.direction(POLL_MS), Direction::Forward);
    zassert_equal(f.update(110, 2 * POLL_MS, false, BACKLASH), 107);
    // A reversal takes two deadbands of raw travel to show.
    zassert_equal(f.update(104, 3 * POLL_MS, false, BACKLASH), 107);
    zassert_equal(f.direction(3 * POLL_MS), Direction::Forward);
    zassert_equal(f.update(103, 4 * POLL_MS, false, BACKLASH), 106);
    zassert_equal(f.direction(4 * POLL_MS), Direction::Reverse);
}

ZTEST(encoder_filter, test_zero_backlash_follows_raw)
{
    Filter f;
    f.update(0, 0, false, 0);
    zassert_equal(f.update(1, POLL_MS, false, 0), 1);
    zassert_equal(f.update(-5, 2 * POLL_MS, false, 0), -5);
    zassert_equal(f.direction(2 * POLL_MS), Direction::Reverse);
}

ZTEST(encoder_filter, test_direction_goes_stale)
{
    Filter f;
    f.update(0, 0, false, 0);
    f.update(10, POLL_MS, false, 0);
    zassert_equal(f.direction(POLL_MS + Filter::STALE_MS), Direction::Forward);
    zassert_equal(f.direction(POLL_MS + Filter::STALE_MS + 1), Direction::Stopped);
}

ZTEST(encoder_filter, test_move_after_a_poll_pause_has_no_direction)
{
    Filter f;
    f.update(0, 0, false, 0);
    int64_t later = Filter::STALE_MS + 1;
    zassert_equal(f.update(500, later, false, 0), 500);
    zassert_equal(f.direction(later), Direction::Stopped);
    f.update(501, later + POLL_MS, false, 0);
    zassert_equal(f.direction(later + POLL_MS), Direction::Forward);
}

ZTEST(encoder_filter, test_changed_settings_restart_the_filter)
{
    Filter f;
    f.update(100, 0, false, BACKLASH);
    f.update(110, POLL_MS, false, BACKLASH);
    zassert_equal(f.update(110, 2 * POLL_MS, false, BACKLASH + 1), 110);
    zassert_equal(f.update(-110, 3 * POLL_MS, true, BACKLASH + 1), -110);
    zassert_equal(f.direction(3 * POLL_MS), Direction::Forward, "a restart is not a move");
}
