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
#include "ModeMath.hpp"

using namespace Mode::Detail;

namespace
{
    constexpr double QUARTER_TURN = 1.57079632679489661923;
}

ZTEST_SUITE(mode_math, NULL, NULL, NULL, NULL, NULL);

// 1600 steps/rev on a 2 mm screw = 800 steps/mm.

ZTEST(mode_math, test_feed_steps_per_sec)
{
    // 60 mm/min = 1 mm/s.
    zassert_equal(feedStepsPerSec(600000, 1600, 20000, 12800), 800);
    zassert_equal(feedStepsPerSec(-600000, 1600, 20000, 12800), 800);
}

ZTEST(mode_math, test_feed_steps_per_sec_limits)
{
    zassert_equal(feedStepsPerSec(60000000, 1600, 20000, 12800), 12800);
    zassert_equal(feedStepsPerSec(1, 1600, 20000, 12800), 1);
    zassert_equal(feedStepsPerSec(0, 1600, 20000, 12800), 1);
}

ZTEST(mode_math, test_vector_feed_splits_the_path_speed)
{
    // 10 mm path at 600 mm/min takes 1 s.
    zassert_equal(vectorFeedStepsPerSec(100000, 6000000, 8000, 20000), 8000);
    zassert_equal(vectorFeedStepsPerSec(100000, 6000000, -8000, 20000), 8000);
    // The main axis covers less of a diagonal: slower.
    zassert_equal(vectorFeedStepsPerSec(200000, 6000000, 8000, 20000), 4000);
}

ZTEST(mode_math, test_vector_feed_limits)
{
    zassert_equal(vectorFeedStepsPerSec(100000, 6000000, 8000, 5000), 5000);
    zassert_equal(vectorFeedStepsPerSec(100000, 0, 8000, 5000), 5000);
    zassert_equal(vectorFeedStepsPerSec(0, 6000000, 8000, 5000), 5000);
    zassert_equal(vectorFeedStepsPerSec(100000000, 1, 1, 5000), 1);
}

ZTEST(mode_math, test_spindle_feed_steps_per_count)
{
    // 2 mm per rev on a 2 mm screw: one motor rev per 2400 counts.
    zassert_within(spindleFeedStepsPerCount(1600, 20000, 20000, 2400), 1600.0 / 2400, 1e-12);
    zassert_within(spindleFeedStepsPerCount(1600, -20000, 20000, 2400), 1600.0 / 2400, 1e-12);
    zassert_within(spindleFeedStepsPerCount(1600, 0, 20000, 2400), 0, 1e-12);
}

ZTEST(mode_math, test_peck_passes)
{
    zassert_equal(peckPasses(10000, 3000), 4);
    zassert_equal(peckPasses(9000, 3000), 3);
    zassert_equal(peckPasses(1000, 3000), 1);
    zassert_equal(peckPasses(10000, 0), 1);
}

ZTEST(mode_math, test_peck_depth)
{
    zassert_equal(peckDepthDu(0, 4, 3000, 10000), 0);
    zassert_equal(peckDepthDu(1, 4, 3000, 10000), 3000);
    zassert_equal(peckDepthDu(3, 4, 3000, 10000), 9000);
    zassert_equal(peckDepthDu(4, 4, 3000, 10000), 10000);
    zassert_equal(peckDepthDu(1, 1, 0, 10000), 10000);
}

ZTEST(mode_math, test_convex_ellipse_runs_face_to_diameter)
{
    double z;
    double x;
    ellipsePoint(3000, 2000, false, 0, z, x);
    zassert_within(z, 0, 1e-6);
    zassert_within(x, -2000, 1e-6);
    ellipsePoint(3000, 2000, false, QUARTER_TURN, z, x);
    zassert_within(z, 3000, 1e-6);
    zassert_within(x, 0, 1e-6);
    for (double angle = 0; angle <= QUARTER_TURN; angle += 0.1) {
        ellipsePoint(3000, 2000, false, angle, z, x);
        double u = (z - 3000) / 3000;
        double v = (x + 2000) / 2000;
        zassert_within(u * u + v * v, 1, 1e-9);
    }
}

ZTEST(mode_math, test_concave_ellipse_runs_face_to_diameter)
{
    double z;
    double x;
    ellipsePoint(3000, 2000, true, 0, z, x);
    zassert_within(z, 0, 1e-6);
    zassert_within(x, -2000, 1e-6);
    ellipsePoint(3000, 2000, true, QUARTER_TURN, z, x);
    zassert_within(z, 3000, 1e-6);
    zassert_within(x, 0, 1e-6);
    for (double angle = 0; angle <= QUARTER_TURN; angle += 0.1) {
        ellipsePoint(3000, 2000, true, angle, z, x);
        zassert_within((z / 3000) * (z / 3000) + (x / 2000) * (x / 2000), 1, 1e-9);
    }
}

ZTEST(mode_math, test_ellipse_speed)
{
    // A circle moves its radius per radian.
    zassert_within(ellipseSpeed(2500, 2500, false, 0.3), 2500, 1e-6);
    zassert_within(ellipseSpeed(2500, 2500, true, 1.1), 2500, 1e-6);
    zassert_within(ellipseSpeed(3000, 2000, false, 0), 2000, 1e-6);
    zassert_within(ellipseSpeed(3000, 2000, false, QUARTER_TURN), 3000, 1e-6);
    zassert_within(ellipseSpeed(3000, 2000, true, 0), 3000, 1e-6);
    zassert_within(ellipseSpeed(0, 0, false, 0.5), 1, 1e-12);
}
