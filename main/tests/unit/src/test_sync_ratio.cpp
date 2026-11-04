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
