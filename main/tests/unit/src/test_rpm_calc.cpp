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
#include "RpmCalc.hpp"

using Rpm::rpmFromDelta;

// Settings.hpp default: encoderPpr=1200, x2 decode -> 2400 counts/rev.

ZTEST_SUITE(rpm_calc, NULL, NULL, NULL, NULL, NULL);

ZTEST(rpm_calc, test_one_rev_per_second_is_60rpm)
{
    int32_t rpm = rpmFromDelta(2400, 1000, 2400);
    zassert_equal(rpm, 60);
}

ZTEST(rpm_calc, test_one_rev_per_half_second_is_120rpm)
{
    int32_t rpm = rpmFromDelta(2400, 500, 2400);
    zassert_equal(rpm, 120);
}

ZTEST(rpm_calc, test_negative_delta_is_negative_rpm)
{
    int32_t rpm = rpmFromDelta(-2400, 1000, 2400);
    zassert_equal(rpm, -60);
}

ZTEST(rpm_calc, test_no_movement_is_zero_rpm)
{
    int32_t rpm = rpmFromDelta(0, 1000, 2400);
    zassert_equal(rpm, 0);
}

ZTEST(rpm_calc, test_zero_elapsed_is_guarded)
{
    int32_t rpm = rpmFromDelta(2400, 0, 2400);
    zassert_equal(rpm, 0);
}
