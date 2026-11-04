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
#include "StepperLimits.hpp"

using Stepper::backlashSteps;
using Stepper::clampToTravel;
using Stepper::maxTravelSteps;

ZTEST_SUITE(stepper_limits, NULL, NULL, NULL, NULL, NULL);

ZTEST(stepper_limits, test_within_travel_unchanged)
{
    int64_t target = clampToTravel(500, 60000);
    zassert_equal(target, 500);
}

ZTEST(stepper_limits, test_clamps_above_travel)
{
    int64_t target = clampToTravel(90000, 60000);
    zassert_equal(target, 60000);
}

ZTEST(stepper_limits, test_clamps_below_travel)
{
    int64_t target = clampToTravel(-90000, 60000);
    zassert_equal(target, -60000);
}

ZTEST(stepper_limits, test_max_travel_steps_default_z_axis)
{
    int64_t steps = maxTravelSteps(300, 800, 40000);
    zassert_equal(steps, 60000);
}

ZTEST(stepper_limits, test_max_travel_steps_zero_is_zero)
{
    int64_t steps = maxTravelSteps(0, 800, 40000);
    zassert_equal(steps, 0);
}

ZTEST(stepper_limits, test_backlash_steps_half_mm)
{
    int64_t steps = backlashSteps(5000, 800, 40000);
    zassert_equal(steps, 100);
}

ZTEST(stepper_limits, test_backlash_steps_zero_is_zero)
{
    int64_t steps = backlashSteps(0, 800, 40000);
    zassert_equal(steps, 0);
}
