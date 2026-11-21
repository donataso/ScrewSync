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
#include "RunView.hpp"

using Display::Detail::RunState;
using Display::Detail::RunView;
using Mode::Id;

ZTEST_SUITE(run_view, NULL, NULL, NULL, NULL, NULL);

ZTEST(run_view, test_closed_until_armed)
{
    RunView view;
    zassert_equal(view.update(Id::Thread, false), RunState::Closed);
    view.arm(Id::Thread);
    zassert_equal(view.update(Id::Thread, false), RunState::Armed);
}

ZTEST(run_view, test_cancel_closes)
{
    RunView view;
    view.arm(Id::Thread);
    view.cancel();
    zassert_equal(view.update(Id::Thread, false), RunState::Closed);
}

ZTEST(run_view, test_job_end_closes)
{
    RunView view;
    view.arm(Id::Turn);
    zassert_equal(view.update(Id::Turn, true), RunState::Running);
    zassert_equal(view.update(Id::Turn, false), RunState::Closed);
}

ZTEST(run_view, test_running_opens_without_arming)
{
    RunView view;
    zassert_equal(view.update(Id::Gear, true), RunState::Running);
}

ZTEST(run_view, test_mode_change_disarms)
{
    RunView view;
    view.arm(Id::Thread);
    zassert_equal(view.update(Id::Turn, false), RunState::Closed);
    zassert_equal(view.update(Id::Thread, false), RunState::Closed);
}

ZTEST(run_view, test_gcode_and_probe_have_none)
{
    RunView view;
    view.arm(Id::Gcode);
    zassert_equal(view.update(Id::Gcode, false), RunState::Closed);
    zassert_equal(view.update(Id::Gcode, true), RunState::Closed);
    zassert_equal(view.update(Id::Probe, true), RunState::Closed);
}
