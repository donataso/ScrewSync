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
#include "WcsRules.hpp"

ZTEST_SUITE(wcs_rules, NULL, NULL, NULL, NULL, NULL);

ZTEST(wcs_rules, test_g54_always)
{
    zassert_is_null(Wcs::refusal(0, false));
    zassert_is_null(Wcs::refusal(0, true));
}

ZTEST(wcs_rules, test_others_need_sensors)
{
    for (size_t i = 1; i < Wcs::COUNT; i++) {
        zassert_not_null(Wcs::refusal(i, false));
        zassert_is_null(Wcs::refusal(i, true));
    }
}

ZTEST(wcs_rules, test_out_of_range)
{
    zassert_not_null(Wcs::refusal(Wcs::COUNT, true));
}
