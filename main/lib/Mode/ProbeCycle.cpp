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
#include "ProbeCycle.hpp"

namespace
{
    using namespace ProbeCycle;

    int64_t absDelta(int64_t a, int64_t b) { return a > b ? a - b : b - a; }

    void touch(State &s, const Params &p, Result &r, int64_t currentPos, bool contactNow, bool justContacted,
               int64_t contactPos, int64_t maxSteps, bool reprobeFeed, Phase next)
    {
        if (justContacted) {
            if (!contactNow) {
                r.error = "contact glitch";
                return;
            }
            s.contactSteps = contactPos;
            s.phase = next;
            s.phaseStart = contactPos;
            return;
        }
        if (absDelta(currentPos, s.phaseStart) >= maxSteps) {
            r.error = "no contact";
            return;
        }
        r.move = true;
        r.reprobeFeed = reprobeFeed;
        r.target = s.phaseStart + p.dir * maxSteps;
    }

    // True once backed off; the contact must be gone by then.
    bool backOff(State &s, const Params &p, Result &r, int64_t currentPos, bool contactNow)
    {
        if (absDelta(currentPos, s.phaseStart) >= p.backOffSteps) {
            if (contactNow) {
                r.error = "contact not lost";
                return false;
            }
            return true;
        }
        r.move = true;
        r.reprobeFeed = true;
        r.target = s.phaseStart - p.dir * p.backOffSteps;
        return false;
    }
}

namespace ProbeCycle
{
    const char *begin(State &s, const Params &, int64_t startPos, bool contactAtStart)
    {
        if (contactAtStart) {
            return "contact present";
        }
        s.phase = Phase::Approach;
        s.phaseStart = startPos;
        s.contactSteps = 0;
        return nullptr;
    }

    bool watching(const State &s) { return s.phase == Phase::Approach || s.phase == Phase::Reprobe; }

    Result tick(State &s, const Params &p, int64_t currentPos, bool contactNow, bool justContacted,
                int64_t contactPos)
    {
        Result r;
        switch (s.phase)
        {
        case Phase::Idle:
        case Phase::Done:
            r.done = true;
            break;

        case Phase::Approach:
            touch(s, p, r, currentPos, contactNow, justContacted, contactPos, p.probeDstSteps, false, Phase::BackOff1);
            break;

        case Phase::BackOff1:
            if (backOff(s, p, r, currentPos, contactNow)) {
                s.phase = Phase::Reprobe;
                s.phaseStart = currentPos;
            }
            break;

        case Phase::Reprobe:
            touch(s, p, r, currentPos, contactNow, justContacted, contactPos, p.backOffSteps + p.marginSteps, true,
                  Phase::BackOff2);
            break;

        case Phase::BackOff2:
            if (backOff(s, p, r, currentPos, contactNow)) {
                s.phase = Phase::Done;
                r.done = true;
                r.surfaceSteps = s.contactSteps + p.dir * p.offsetSteps;
            }
            break;
        }
        return r;
    }
}
