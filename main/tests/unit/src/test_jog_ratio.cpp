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

using Jog::jogTarget;

// Settings.hpp defaults: 800 motor steps/rev, 600 handwheel pulses/rev.

ZTEST_SUITE(jog_ratio, NULL, NULL, NULL, NULL, NULL);

ZTEST(jog_ratio, test_target_unchanged_at_origin)
{
    int64_t target = jogTarget(1000, 500, 500, 800, 600.0f, 1.0f);
    zassert_equal(target, 1000);
}

ZTEST(jog_ratio, test_one_handwheel_turn_moves_one_motor_revolution)
{
    int64_t target = jogTarget(0, 0, 600, 800, 600.0f, 1.0f);
    zassert_equal(target, 800);
}

ZTEST(jog_ratio, test_coarse_multiplier_scales_up)
{
    int64_t target = jogTarget(0, 0, 600, 800, 600.0f, 10.0f);
    zassert_equal(target, 8000);
}

ZTEST(jog_ratio, test_fine_multiplier_scales_down)
{
    int64_t target = jogTarget(0, 0, 600, 800, 600.0f, 0.1f);
    zassert_equal(target, 80);
}
