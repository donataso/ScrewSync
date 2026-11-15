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
#include "HomingSeq.hpp"

using namespace HomingSeq;

namespace
{
    constexpr int64_t SWITCH_AT = 5000;
    constexpr int64_t SEARCH = 20000;
    constexpr int64_t LATCH_BACK_OFF = 200;
    constexpr int64_t PULL_OFF = 100;
    constexpr Params PARAMS = {SEARCH, 2 * LATCH_BACK_OFF, LATCH_BACK_OFF, PULL_OFF, 1000, 100};

    // The switch is active at and beyond SWITCH_AT, or never (stuck: always).
    struct Rig
    {
        int64_t position;
        bool latched = false;
        int64_t latchPosition = 0;
        bool arrived = true;
        int64_t target = 0;
        Watch watch = Watch::None;
        bool noSwitch = false;
        bool stuckActive = false;
        int moves = 0;
        int32_t speeds[8] = {};

        bool active(int64_t pos) const
        {
            return stuckActive || (!noSwitch && pos >= SWITCH_AT);
        }

        void execute(const Output &out)
        {
            speeds[moves] = out.speed;
            moves++;
            watch = out.watch;
            latched = false;
            int64_t dir = out.target > position ? 1 : -1;
            int64_t stop = out.target;
            if (watch != Watch::None && !stuckActive) {
                for (int64_t p = position; p != out.target; p += dir) {
                    bool now = active(p + dir);
                    bool hit = watch == Watch::Contact ? now : !now;
                    if (hit) {
                        stop = p + dir;
                        latched = true;
                        latchPosition = stop;
                        break;
                    }
                }
            }
            position = stop;
            target = out.target;
            arrived = true;
        }

        Output step(Sequence &seq)
        {
            Input in = {active(position), latched, latchPosition, arrived, position};
            Output out = seq.tick(in);
            if (out.move) {
                execute(out);
            }
            return out;
        }
    };

    State run(Rig &rig, Sequence &seq, const char *&error)
    {
        error = nullptr;
        for (int i = 0; i < 50; i++) {
            Output out = rig.step(seq);
            if (out.error) {
                error = out.error;
            }
            if (seq.state() == State::Done || seq.state() == State::Failed) {
                break;
            }
        }
        return seq.state();
    }
}

ZTEST_SUITE(homing_seq, NULL, NULL, NULL, NULL, NULL);

ZTEST(homing_seq, test_normal_run)
{
    Rig rig = {.position = 0};
    Sequence seq;
    seq.begin(PARAMS, rig.active(rig.position));
    const char *error;
    zassert_equal(run(rig, seq, error), State::Done);
    zassert_is_null(error);
    zassert_equal(rig.moves, 4);
    zassert_equal(rig.position, SWITCH_AT - PULL_OFF);
    zassert_equal(rig.speeds[0], 1000);
    zassert_equal(rig.speeds[1], 1000);
    zassert_equal(rig.speeds[2], 100);
    zassert_equal(rig.speeds[3], 100);
}

ZTEST(homing_seq, test_start_on_switch_backs_off_first)
{
    Rig rig = {.position = SWITCH_AT + 300};
    Sequence seq;
    seq.begin(PARAMS, rig.active(rig.position));
    zassert_equal(seq.state(), State::BackOffSwitch);
    const char *error;
    zassert_equal(run(rig, seq, error), State::Done);
    zassert_is_null(error);
    zassert_equal(rig.moves, 5);
    zassert_equal(rig.position, SWITCH_AT - PULL_OFF);
}

ZTEST(homing_seq, test_switch_not_found)
{
    Rig rig = {.position = 0, .noSwitch = true};
    Sequence seq;
    seq.begin(PARAMS, false);
    const char *error;
    zassert_equal(run(rig, seq, error), State::Failed);
    zassert_not_null(error);
    zassert_equal(rig.target, SEARCH);
}

ZTEST(homing_seq, test_switch_still_active_after_pull_off)
{
    Rig rig = {.position = 0};
    Sequence seq;
    seq.begin(PARAMS, false);
    for (int i = 0; i < 50 && seq.state() != State::CheckClear; i++) {
        rig.step(seq);
    }
    zassert_equal(seq.state(), State::CheckClear);
    rig.stuckActive = true;
    Output out = rig.step(seq);
    zassert_equal(seq.state(), State::Failed);
    zassert_not_null(out.error);
}

ZTEST(homing_seq, test_switch_does_not_release)
{
    Rig rig = {.position = SWITCH_AT + 300, .stuckActive = true};
    Sequence seq;
    seq.begin(PARAMS, true);
    const char *error;
    zassert_equal(run(rig, seq, error), State::Failed);
    zassert_not_null(error);
}

ZTEST(homing_seq, test_waits_until_arrived)
{
    Sequence seq;
    seq.begin(PARAMS, false);
    Input in = {false, false, 0, true, 0};
    zassert_true(seq.tick(in).move);
    in.arrived = false;
    Output out = seq.tick(in);
    zassert_false(out.move);
    zassert_is_null(out.error);
    zassert_equal(seq.state(), State::Fast);
}

ZTEST(homing_seq, test_queue_without_x_axis)
{
    Stepper::Axis axes[2];
    zassert_equal(queue(false, axes), 1);
    zassert_equal(axes[0], Stepper::Axis::Z);
}

ZTEST(homing_seq, test_queue_x_first)
{
    Stepper::Axis axes[2];
    zassert_equal(queue(true, axes), 2);
    zassert_equal(axes[0], Stepper::Axis::X);
    zassert_equal(axes[1], Stepper::Axis::Z);
}

ZTEST(homing_seq, test_edge_idle_moving_active_trips)
{
    zassert_equal(edgeAction(false, true, true, Watch::None, true), EdgeAction::Estop);
    zassert_equal(edgeAction(false, false, true, Watch::None, true), EdgeAction::Estop);
}

ZTEST(homing_seq, test_edge_idle_ignored)
{
    zassert_equal(edgeAction(false, true, true, Watch::None, false), EdgeAction::Ignore);
    zassert_equal(edgeAction(false, true, false, Watch::None, true), EdgeAction::Ignore);
}

ZTEST(homing_seq, test_edge_homing_other_axis)
{
    zassert_equal(edgeAction(true, false, true, Watch::Contact, true), EdgeAction::Estop);
    zassert_equal(edgeAction(true, false, false, Watch::Contact, true), EdgeAction::Ignore);
}

ZTEST(homing_seq, test_edge_homing_watched_axis)
{
    zassert_equal(edgeAction(true, true, true, Watch::Contact, true), EdgeAction::Latch);
    zassert_equal(edgeAction(true, true, false, Watch::Release, true), EdgeAction::Latch);
    zassert_equal(edgeAction(true, true, false, Watch::Contact, true), EdgeAction::Ignore);
    zassert_equal(edgeAction(true, true, true, Watch::Release, true), EdgeAction::Ignore);
    zassert_equal(edgeAction(true, true, true, Watch::None, true), EdgeAction::Ignore);
}
