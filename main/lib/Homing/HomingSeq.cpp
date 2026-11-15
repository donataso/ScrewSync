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
#include "HomingSeq.hpp"

namespace
{
    constexpr const char *NOT_FOUND = "home switch not found";
    constexpr const char *STUCK = "home switch does not release";
    constexpr const char *NOT_CLEAR = "home switch not clear after pull-off";
}

namespace HomingSeq
{
    EdgeAction edgeAction(bool homing, bool watchedAxis, bool active, Watch watch, bool moving)
    {
        if (!homing) {
            return active && moving ? EdgeAction::Estop : EdgeAction::Ignore;
        }
        if (!watchedAxis) {
            return active ? EdgeAction::Estop : EdgeAction::Ignore;
        }
        if (watch == Watch::None || active != (watch == Watch::Contact)) {
            return EdgeAction::Ignore;
        }
        return EdgeAction::Latch;
    }

    size_t queue(bool hasXMotor, Stepper::Axis (&out)[2])
    {
        size_t count = 0;
        if (hasXMotor) {
            out[count++] = Stepper::Axis::X;
        }
        out[count++] = Stepper::Axis::Z;
        return count;
    }

    void Sequence::begin(const Params &params, bool switchActive)
    {
        params_ = params;
        state_ = switchActive ? State::BackOffSwitch : State::Fast;
        issued_ = false;
    }

    State Sequence::state() const
    {
        return state_;
    }

    Output Sequence::enter(State next, const Input &in)
    {
        state_ = next;
        issued_ = true;
        switch (next) {
        case State::BackOffSwitch:
            return {true, in.position - params_.searchSteps, params_.slowSpeed, Watch::Release, nullptr};
        case State::Fast:
            return {true, in.position + params_.searchSteps, params_.fastSpeed, Watch::Contact, nullptr};
        case State::LatchBackOff:
            return {true, in.latchPosition - params_.latchBackOffSteps, params_.fastSpeed, Watch::None, nullptr};
        case State::Slow:
            return {true, in.position + params_.slowSearchSteps, params_.slowSpeed, Watch::Contact, nullptr};
        case State::PullOff:
            return {true, in.latchPosition - params_.pullOffSteps, params_.slowSpeed, Watch::None, nullptr};
        default:
            return {false, 0, 0, Watch::None, nullptr};
        }
    }

    Output Sequence::tick(const Input &in)
    {
        if (state_ == State::Done || state_ == State::Failed) {
            return {false, 0, 0, Watch::None, nullptr};
        }
        if (!issued_) {
            return enter(state_, in);
        }
        if (state_ == State::CheckClear) {
            if (in.switchActive) {
                state_ = State::Failed;
                return {false, 0, 0, Watch::None, NOT_CLEAR};
            }
            state_ = State::Done;
            return {false, 0, 0, Watch::None, nullptr};
        }
        if (!in.arrived) {
            return {false, 0, 0, Watch::None, nullptr};
        }
        switch (state_) {
        case State::BackOffSwitch:
            if (!in.latched) {
                state_ = State::Failed;
                return {false, 0, 0, Watch::None, STUCK};
            }
            return enter(State::Fast, in);
        case State::Fast:
        case State::Slow:
            if (!in.latched) {
                state_ = State::Failed;
                return {false, 0, 0, Watch::None, NOT_FOUND};
            }
            return enter(state_ == State::Fast ? State::LatchBackOff : State::PullOff, in);
        case State::LatchBackOff:
            return enter(State::Slow, in);
        case State::PullOff:
            state_ = State::CheckClear;
            return {false, 0, 0, Watch::None, nullptr};
        default:
            return {false, 0, 0, Watch::None, nullptr};
        }
    }
}
