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
#include "JogRatio.hpp"

using Jog::jogSteps;

// 800 motor steps/rev, 600 handwheel pulses/rev, 40000 du screw pitch.

ZTEST_SUITE(jog_ratio, NULL, NULL, NULL, NULL, NULL);

ZTEST(jog_ratio, test_no_count_change_no_steps)
{
    double remainder = 0;
    zassert_equal(jogSteps(0, 800, 600.0f, 40000, 40000, remainder), 0);
    zassert_equal(remainder, 0.0);
}

ZTEST(jog_ratio, test_travel_equal_to_pitch_moves_one_motor_revolution)
{
    double remainder = 0;
    zassert_equal(jogSteps(600, 800, 600.0f, 40000, 40000, remainder), 800);
}

ZTEST(jog_ratio, test_negative_turn_moves_back)
{
    double remainder = 0;
    zassert_equal(jogSteps(-600, 800, 600.0f, 40000, 40000, remainder), -800);
}

ZTEST(jog_ratio, test_larger_travel_scales_up)
{
    double remainder = 0;
    zassert_equal(jogSteps(600, 800, 600.0f, 400000, 40000, remainder), 8000);
}

ZTEST(jog_ratio, test_smaller_travel_scales_down)
{
    double remainder = 0;
    zassert_equal(jogSteps(600, 800, 600.0f, 4000, 40000, remainder), 80);
}

ZTEST(jog_ratio, test_single_counts_with_small_travel_add_up)
{
    double remainder = 0;
    int64_t total = 0;
    for (int i = 0; i < 600; i++) {
        total += jogSteps(1, 800, 600.0f, 4000, 40000, remainder);
    }
    zassert_within(total, 80, 1, "got %lld", (long long)total);
}

ZTEST(jog_ratio, test_there_and_back_returns_to_start)
{
    double remainder = 0;
    int64_t total = 0;
    for (int i = 0; i < 250; i++) {
        total += jogSteps(1, 800, 600.0f, 40000, 40000, remainder);
    }
    for (int i = 0; i < 250; i++) {
        total += jogSteps(-1, 800, 600.0f, 40000, 40000, remainder);
    }
    zassert_equal(total, 0, "got %lld", (long long)total);
}
