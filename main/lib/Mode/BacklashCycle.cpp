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
#include "BacklashCycle.hpp"

namespace
{
    using namespace BacklashCycle;

    int64_t absDelta(int64_t a, int64_t b)
    {
        return a > b ? a - b : b - a;
    }

    void approach(State &s, const Params &p, const Input &in, Result &r)
    {
        if (in.latched) {
            if (!in.contactNow) {
                r.error = "contact glitch";
                return;
            }
            s.contactMotor = in.latchedMotor;
            s.phase = Phase::Release;
            s.phaseStart = in.latchedPos;
            return;
        }
        bool first = s.run == 0;
        int64_t limit = first ? p.probeDstSteps : p.backOffSteps + p.marginSteps;
        if (absDelta(in.pos, s.phaseStart) >= limit) {
            r.error = "no contact";
            return;
        }
        r.move = true;
        r.reprobeFeed = !first;
        r.target = s.phaseStart + p.dir * limit;
    }

    void release(State &s, const Params &p, const Input &in, Result &r)
    {
        if (in.latched) {
            if (in.contactNow) {
                r.error = "release glitch";
                return;
            }
            s.samples[s.run++] = absDelta(in.latchedMotor, s.contactMotor);
            s.phase = Phase::BackOff;
            s.phaseStart = in.latchedPos;
            return;
        }
        if (absDelta(in.motorPos, s.contactMotor) >= p.backOffSteps) {
            r.error = "no release";
            return;
        }
        r.move = true;
        r.reprobeFeed = true;
        r.target = s.phaseStart - p.dir * p.backOffSteps;
    }

    void backOff(State &s, const Params &p, const Input &in, Result &r)
    {
        if (absDelta(in.pos, s.phaseStart) < p.backOffSteps) {
            r.move = true;
            r.reprobeFeed = true;
            r.target = s.phaseStart - p.dir * p.backOffSteps;
            return;
        }
        if (in.contactNow) {
            r.error = "contact not lost";
            return;
        }
        if (s.run < p.runs) {
            s.phase = Phase::Approach;
            s.phaseStart = in.pos;
            return;
        }
        s.phase = Phase::Done;
        r.done = true;
        r.backlashSteps = median(s.samples, s.run);
    }
}

namespace BacklashCycle
{
    const char *begin(State &s, const Params &p, int64_t startPos, bool contactAtStart)
    {
        if (contactAtStart) {
            return "contact present";
        }
        if (p.runs < 1 || p.runs > MAX_RUNS) {
            return "bad run count";
        }
        s = State{};
        s.phase = Phase::Approach;
        s.phaseStart = startPos;
        return nullptr;
    }

    Watch watching(const State &s)
    {
        switch (s.phase) {
        case Phase::Approach:
            return Watch::Contact;
        case Phase::Release:
            return Watch::Release;
        default:
            return Watch::None;
        }
    }

    Result tick(State &s, const Params &p, const Input &in)
    {
        Result r;
        switch (s.phase) {
        case Phase::Idle:
        case Phase::Done:
            r.done = true;
            break;
        case Phase::Approach:
            approach(s, p, in, r);
            break;
        case Phase::Release:
            release(s, p, in, r);
            break;
        case Phase::BackOff:
            backOff(s, p, in, r);
            break;
        }
        return r;
    }

    int64_t median(int64_t *samples, int count)
    {
        for (int i = 1; i < count; i++) {
            int64_t v = samples[i];
            int j = i;
            for (; j > 0 && samples[j - 1] > v; j--) {
                samples[j] = samples[j - 1];
            }
            samples[j] = v;
        }
        if (count == 0) {
            return 0;
        }
        int mid = count / 2;
        return count % 2 ? samples[mid] : (samples[mid - 1] + samples[mid]) / 2;
    }
}
