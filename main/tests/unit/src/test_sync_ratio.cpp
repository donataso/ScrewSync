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
#include "SyncRatio.hpp"

using Sync::ratioTarget;

// Settings.hpp defaults: motorSteps=800 steps/rev, screwPitchDu=40000 (4mm/rev).
// encoderStepsPerRev=2400 (encoderPpr=1200, x2 decode) -- one full spindle turn.

ZTEST_SUITE(sync_ratio, NULL, NULL, NULL, NULL, NULL);

ZTEST(sync_ratio, test_target_unchanged_at_origin)
{
    int64_t target = ratioTarget(1000, 500, 500, 800, 4000, 1, 40000, 2400);
    zassert_equal(target, 1000);
}

ZTEST(sync_ratio, test_target_scales_by_feed_over_pitch)
{
    // one spindle turn, 0.4mm/rev feed over a 4mm/rev screw -> 800*0.1=80 steps
    int64_t target = ratioTarget(0, 0, 2400, 800, 4000, 1, 40000, 2400);
    zassert_equal(target, 80);
}

ZTEST(sync_ratio, test_negative_dupr_reverses_direction)
{
    int64_t target = ratioTarget(0, 0, 2400, 800, -4000, 1, 40000, 2400);
    zassert_equal(target, -80);
}

ZTEST(sync_ratio, test_starts_multiplies_feed)
{
    // multi-start thread: same dupr, 2 starts -> twice the steps per spindle turn
    int64_t target = ratioTarget(0, 0, 2400, 800, 4000, 2, 40000, 2400);
    zassert_equal(target, 160);
}

ZTEST(sync_ratio, test_helix_phase_at_axis_zero_is_origin)
{
    zassert_equal(Sync::helixPhase(37, 0, -1.0 / 30), 37);
}

ZTEST(sync_ratio, test_helix_phase_puts_ref_axis_on_helix)
{
    // 80 steps per turn (0.4mm/rev on a 4mm screw): axis 40 is half a turn off.
    int64_t phase = Sync::helixPhase(37, 40, Sync::ratio(800, 4000, 1, 40000, 2400));
    int64_t atOrigin = ratioTarget(40, phase, 37, 800, 4000, 1, 40000, 2400);
    zassert_equal(((atOrigin % 80) + 80) % 80, 0);
}

ZTEST(sync_ratio, test_next_on_phase_forward)
{
    zassert_equal(Sync::nextOnPhase(100, 150, 2400, 1), 150);
    zassert_equal(Sync::nextOnPhase(100, 50, 2400, 1), 2450);
    zassert_equal(Sync::nextOnPhase(100, 100 - 2400 * 3, 2400, 1), 100);
}

ZTEST(sync_ratio, test_next_on_phase_reverse)
{
    zassert_equal(Sync::nextOnPhase(100, 50, 2400, -1), 50);
    zassert_equal(Sync::nextOnPhase(100, 150, 2400, -1), 150 - 2400);
}

ZTEST(sync_ratio, test_crossed_phase)
{
    int64_t at = 0;
    zassert_true(Sync::crossedPhase(100, 200, 150 + 2400, 2400, at));
    zassert_equal(at, 150);
    zassert_true(Sync::crossedPhase(100, 150, 150, 2400, at));
    zassert_false(Sync::crossedPhase(100, 149, 150, 2400, at));
    zassert_false(Sync::crossedPhase(150, 160, 150, 2400, at));
    zassert_true(Sync::crossedPhase(200, 100, 150, 2400, at));
    zassert_equal(at, 150);
    zassert_false(Sync::crossedPhase(100, 100, 100, 2400, at));
}

ZTEST(sync_ratio, test_reruns_land_in_same_groove)
{
    double perCount = Sync::ratio(800, 4000, 1, 40000, 2400);
    int64_t phase = Sync::helixPhase(37, 0, perCount);
    int64_t originA = Sync::nextOnPhase(500, phase, 2400, 1);
    int64_t originB = Sync::nextOnPhase(1900 + 2400 * 5, phase, 2400, 1);
    for (int64_t s = 20000; s < 22400; s += 300) {
        int64_t a = ratioTarget(0, originA, s, 800, 4000, 1, 40000, 2400);
        int64_t b = ratioTarget(0, originB, s, 800, 4000, 1, 40000, 2400);
        zassert_equal((a - b) % 80, 0);
    }
}

ZTEST(sync_ratio, test_axis_at_reference_starts_on_phase_at_angle_zero)
{
    double perCount = Sync::ratio(800, 4000, 1, 40000, 2400);
    int64_t angleZeroCount = 777;
    int64_t phase = Sync::helixPhase(angleZeroCount, 0, perCount);
    zassert_equal(Sync::nextOnPhase(angleZeroCount, phase, 2400, 1), angleZeroCount);
    zassert_equal(Sync::nextOnPhase(angleZeroCount, phase, 2400, -1), angleZeroCount);
}

ZTEST(sync_ratio, test_ramp_half_counts)
{
    // 0.5 steps/count at 1000 counts/s is 500 steps/s; at 1000 steps/s^2 the
    // ramp is 125 steps over 0.5 s, half of it 250 counts.
    zassert_within(Sync::rampHalfCounts(0.5, 1000, 1000), 250, 1e-9);
    zassert_within(Sync::rampHalfCounts(-0.5, 1000, 1000), 250, 1e-9);
    zassert_equal(Sync::rampHalfCounts(0.5, 1000, 0), 0);
}

ZTEST(sync_ratio, test_ramp_offset_joins_helix_tangentially)
{
    double k = -0.5;
    double half = 250;
    zassert_equal(Sync::rampOffset(-300, half, k), 0);
    zassert_equal(Sync::rampOffset(-half, half, k), 0);
    zassert_within(Sync::rampOffset(half, half, k), k * half, 1e-9);
    zassert_within(Sync::rampOffset(400, half, k), k * 400, 1e-9);
    double slope = Sync::rampOffset(half - 0.001, half, k) - Sync::rampOffset(half - 0.002, half, k);
    zassert_within(slope / 0.001, k, 1e-3);
    // Constant acceleration: the second difference is the same everywhere on the ramp.
    double a = Sync::rampOffset(-100, half, k) - 2 * Sync::rampOffset(-99, half, k) + Sync::rampOffset(-98, half, k);
    double b = Sync::rampOffset(100, half, k) - 2 * Sync::rampOffset(101, half, k) + Sync::rampOffset(102, half, k);
    zassert_within(a, b, 1e-9);
}

ZTEST(sync_ratio, test_ramp_offset_without_ramp_is_helix)
{
    zassert_equal(Sync::rampOffset(-10, 0, 0.5), 0);
    zassert_within(Sync::rampOffset(10, 0, 0.5), 5, 1e-9);
}
