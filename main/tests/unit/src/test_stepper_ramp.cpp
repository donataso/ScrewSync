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
#include "StepperRamp.hpp"

using Stepper::nextRampSpeed;

// All cases use Settings.hpp's defaults: accel=20000 steps/s^2,
// speedStart=800 steps/s, maxSpeed=6400 steps/s.

ZTEST_SUITE(stepper_ramp, NULL, NULL, NULL, NULL, NULL);

ZTEST(stepper_ramp, test_accelerates_when_far_from_target)
{
    // dt=1250us at 800 steps/s, dv=20000*1250/1e6=25, stoppingDistance=16
    int32_t speed = nextRampSpeed(800, 1000000, 20000, 800, 6400);
    zassert_equal(speed, 825);
}

ZTEST(stepper_ramp, test_decelerates_near_target)
{
    // dt=156us at 6400 steps/s, dv=20000*156/1e6=3, stoppingDistance=1024
    int32_t speed = nextRampSpeed(6400, 5, 20000, 800, 6400);
    zassert_equal(speed, 6397);
}

ZTEST(stepper_ramp, test_clamps_to_speed_start)
{
    // decelerating from the floor would undershoot to 775 -- clamped back up
    int32_t speed = nextRampSpeed(800, 1, 20000, 800, 6400);
    zassert_equal(speed, 800);
}

ZTEST(stepper_ramp, test_clamps_to_max_speed)
{
    // accelerating from the ceiling would overshoot to 6403 -- clamped back down
    int32_t speed = nextRampSpeed(6400, 1000000, 20000, 800, 6400);
    zassert_equal(speed, 6400);
}

ZTEST(stepper_ramp, test_decelerates_to_lowered_max_speed)
{
    // dt=156us at 6400 steps/s, dv=3 -- steps down instead of snapping to 3200
    int32_t speed = nextRampSpeed(6400, 1000000, 20000, 800, 3200);
    zassert_equal(speed, 6397);
}
