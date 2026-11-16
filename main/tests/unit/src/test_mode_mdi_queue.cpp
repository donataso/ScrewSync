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
#include <string.h>
#include "ModeMdiQueue.hpp"

using namespace Mode::Detail;

namespace
{
    Gcode::Step motion(int32_t zDu)
    {
        Gcode::Step step = {};
        step.kind = Gcode::StepKind::Feed;
        step.zDu = zDu;
        return step;
    }
}

ZTEST_SUITE(mode_mdi_queue, nullptr, nullptr, nullptr, nullptr, nullptr);

ZTEST(mode_mdi_queue, test_print_reaches_head_after_earlier_steps)
{
    MdiQueue queue;
    queue.push(motion(1));
    queue.pushPrint("done");
    queue.push(motion(2));
    zassert_equal(queue.count(), 3);
    zassert_equal(queue.head().kind, Gcode::StepKind::Feed);
    queue.pop();
    zassert_equal(queue.head().kind, Gcode::StepKind::Print);
    zassert_str_equal(queue.headText(), "done");
    queue.pop();
    zassert_equal(queue.head().zDu, 2);
}

ZTEST(mode_mdi_queue, test_print_uses_one_slot)
{
    MdiQueue queue;
    zassert_equal(queue.free(), MdiQueue::LEN);
    queue.pushPrint("a");
    zassert_equal(queue.free(), MdiQueue::LEN - 1);
    queue.pop();
    zassert_equal(queue.free(), MdiQueue::LEN);
}

ZTEST(mode_mdi_queue, test_texts_stay_in_order_when_wrapping)
{
    MdiQueue queue;
    for (int round = 0; round < 3; round++) {
        queue.push(motion(0));
        queue.pushPrint("first");
        queue.pushPrint("second");
        queue.pop();
        zassert_str_equal(queue.headText(), "first");
        queue.pop();
        zassert_str_equal(queue.headText(), "second");
        queue.pop();
        zassert_equal(queue.count(), 0);
    }
}

ZTEST(mode_mdi_queue, test_completed_counts_pops_not_clears)
{
    MdiQueue queue;
    queue.push(motion(1));
    queue.pushPrint("a");
    queue.push(motion(2));
    zassert_equal(queue.completed(), 0);
    queue.pop();
    queue.pop();
    zassert_equal(queue.completed(), 2);
    queue.clear();
    zassert_equal(queue.completed(), 2);
}

ZTEST(mode_mdi_queue, test_clear_drops_prints)
{
    MdiQueue queue;
    queue.pushPrint("old");
    queue.push(motion(1));
    queue.clear();
    zassert_equal(queue.count(), 0);
    queue.pushPrint("new");
    zassert_str_equal(queue.headText(), "new");
}
