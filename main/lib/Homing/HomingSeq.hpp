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
#ifndef HomingSeq_h
#define HomingSeq_h

#include "Stepper.hpp"
#include <stddef.h>
#include <stdint.h>

namespace HomingSeq
{
    enum class State : uint8_t
    {
        BackOffSwitch,
        Fast,
        LatchBackOff,
        Slow,
        PullOff,
        CheckClear,
        Done,
        Failed,
    };

    enum class Watch : uint8_t
    {
        None,
        Contact,
        Release,
    };

    struct Params
    {
        int64_t searchSteps;
        int64_t slowSearchSteps;
        int64_t latchBackOffSteps;
        int64_t pullOffSteps;
        int32_t fastSpeed;
        int32_t slowSpeed;
    };

    struct Input
    {
        bool switchActive;
        bool latched;
        int64_t latchPosition;
        bool arrived;
        int64_t position;
    };

    struct Output
    {
        bool move;
        int64_t target;
        int32_t speed;
        Watch watch;
        const char *error;
    };

    size_t queue(bool hasXMotor, Stepper::Axis (&out)[2]);

    class Sequence
    {
    public:
        void begin(const Params &params, bool switchActive);
        Output tick(const Input &in);
        State state() const;

    private:
        Output enter(State next, const Input &in);

        Params params_ = {};
        State state_ = State::Done;
        bool issued_ = false;
    };
}

#endif
