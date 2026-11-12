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
using Stepper::runAheadDistance;
using Stepper::stoppingDistance;

// All cases use Settings.hpp's defaults: accel=20000 steps/s^2,
// speedStart=800 steps/s, maxSpeed=6400 steps/s.

static int32_t g_remainder;

static void resetRemainder(void *)
{
    g_remainder = 0;
}

ZTEST_SUITE(stepper_ramp, NULL, NULL, resetRemainder, NULL, NULL);

ZTEST(stepper_ramp, test_accelerates_when_far_from_target)
{
    // dt=1250us at 800 steps/s, dv=20000*1250/1e6=25, stoppingDistance=16
    int32_t speed = nextRampSpeed(800, 1000000, 20000, 20000, 800, 6400, g_remainder);
    zassert_equal(speed, 825);
}

ZTEST(stepper_ramp, test_decelerates_near_target)
{
    // stoppingDistance=1024, sqrt(2*20000*1000)=6324
    int32_t speed = nextRampSpeed(6400, 1000, 20000, 20000, 800, 6400, g_remainder);
    zassert_equal(speed, 6324);
}

ZTEST(stepper_ramp, test_clamps_to_speed_start)
{
    // sqrt(2*20000*1)=200 -- clamped back up
    int32_t speed = nextRampSpeed(800, 1, 20000, 20000, 800, 6400, g_remainder);
    zassert_equal(speed, 800);
}

ZTEST(stepper_ramp, test_clamps_to_max_speed)
{
    // accelerating from the ceiling would overshoot to 6403 -- clamped back down
    int32_t speed = nextRampSpeed(6400, 1000000, 20000, 20000, 800, 6400, g_remainder);
    zassert_equal(speed, 6400);
}

ZTEST(stepper_ramp, test_decelerates_to_lowered_max_speed)
{
    // dt=156us at 6400 steps/s, dv=3 -- steps down instead of snapping to 3200
    int32_t speed = nextRampSpeed(6400, 1000000, 20000, 20000, 800, 3200, g_remainder);
    zassert_equal(speed, 6397);
}

// Unequal rates: accel=10000, decel=40000 steps/s^2.

ZTEST(stepper_ramp, test_speeds_up_at_acceleration)
{
    // dt=1250us, dv=10000*1250/1e6=12
    int32_t speed = nextRampSpeed(800, 1000000, 10000, 40000, 800, 6400, g_remainder);
    zassert_equal(speed, 812);
}

ZTEST(stepper_ramp, test_slows_down_at_deceleration)
{
    // sqrt(2*40000*500)=6324
    int32_t speed = nextRampSpeed(6400, 500, 10000, 40000, 800, 6400, g_remainder);
    zassert_equal(speed, 6324);
}

ZTEST(stepper_ramp, test_brakes_at_deceleration_stopping_distance)
{
    // stoppingDistance=6400^2/(2*40000)=512
    zassert_equal(nextRampSpeed(6400, 513, 10000, 40000, 800, 6400, g_remainder), 6400);
    zassert_equal(nextRampSpeed(6400, 511, 10000, 40000, 800, 6400, g_remainder), 6393);
}

ZTEST(stepper_ramp, test_lowered_max_speed_uses_deceleration)
{
    // dt=156us, dv=40000*156/1e6=6
    int32_t speed = nextRampSpeed(6400, 1000000, 10000, 40000, 800, 3200, g_remainder);
    zassert_equal(speed, 6394);
}

ZTEST(stepper_ramp, test_stopping_distance_scales_with_deceleration)
{
    zassert_equal(stoppingDistance(6400, 20000), 1024);
    zassert_equal(stoppingDistance(6400, 40000), 512);
}

ZTEST(stepper_ramp, test_run_ahead_speeds_up_at_acceleration_stops_at_deceleration)
{
    // reachable=800+10000*0.2=2800, cruise=2800*0.2=560, stop=2800^2/(2*40000)=98
    zassert_equal(runAheadDistance(800, 10000, 40000, 6400, 200000), 659);
}

ZTEST(stepper_ramp, test_run_ahead_clamps_to_max_speed)
{
    // cruise=6400*0.2=1280, stop=512
    zassert_equal(runAheadDistance(6000, 10000, 40000, 6400, 200000), 1793);
}

ZTEST(stepper_ramp, test_full_move_ramps_down_faster_than_up)
{
    const int64_t target = 20000;
    int32_t speed = 800;
    int32_t peak = speed;
    int64_t stepsUp = 0;
    int64_t stepsDown = 0;
    for (int64_t pos = 1; pos < target; pos++) {
        int32_t next = nextRampSpeed(speed, target - pos, 10000, 40000, 800, 6400, g_remainder);
        if (next > speed) {
            stepsUp++;
        } else if (next < speed) {
            stepsDown++;
        }
        speed = next;
        if (speed > peak) {
            peak = speed;
        }
    }
    zassert_equal(peak, 6400);
    zassert_equal(speed, 800, "arrived at %d", speed);
    zassert_true(stepsDown * 3 < stepsUp, "up %lld down %lld", stepsUp, stepsDown);
}

ZTEST(stepper_ramp, test_target_inside_stopping_distance_drops_to_speed_start)
{
    int32_t speed = nextRampSpeed(6400, 5, 10000, 40000, 800, 6400, g_remainder);
    zassert_equal(speed, 800);
}

ZTEST(stepper_ramp, test_full_move_arrives_at_speed_start_with_equal_rates)
{
    const int64_t target = 200000;
    int32_t speed = 800;
    for (int64_t pos = 1; pos < target; pos++) {
        speed = nextRampSpeed(speed, target - pos, 10000, 10000, 800, 12800, g_remainder);
    }
    zassert_equal(speed, 800, "arrived at %d", speed);
}

ZTEST(stepper_ramp, test_carries_speed_fraction_between_steps)
{
    // dt=156us, dv=10000*156/1e6=1.56 per step
    zassert_equal(nextRampSpeed(6400, 1000000, 10000, 40000, 800, 12800, g_remainder), 6401);
    zassert_equal(g_remainder, 560000);
    zassert_equal(nextRampSpeed(6401, 1000000, 10000, 40000, 800, 12800, g_remainder), 6403);
    zassert_equal(g_remainder, 120000);
}

ZTEST(stepper_ramp, test_speed_up_takes_the_kinematic_distance)
{
    // (6400^2-800^2)/(2*10000)=2016 steps
    int32_t speed = 800;
    int64_t steps = 0;
    while (speed < 6400) {
        speed = nextRampSpeed(speed, 1000000, 10000, 40000, 800, 6400, g_remainder);
        steps++;
    }
    zassert_within(steps, 2016, 20, "took %lld steps", steps);
}
