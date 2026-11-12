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
#include "ModePass.hpp"
#include "Settings.hpp"
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::detail;

    enum class Phase : uint8_t
    {
        Return,  // main to just short of start
        Settle,  // main onto start in the cut direction
        Plunge,  // aux to pass depth
        Cut,
        Retract,
        Home,    // aux back to the engage point
    };

    void enter(PassJob &j, Phase phase)
    {
        j.phase = (uint8_t)phase;
        j.entered = false;
    }

    int32_t totalLevels(const PassJob &j)
    {
        return j.levels + j.finishLevels;
    }

    double depthFraction(const PassJob &j, int32_t level)
    {
        double fraction = (double)MIN(level, j.levels) / j.levels;
        if (j.squaredDepth) {
            fraction = 1 - (1 - fraction) * (1 - fraction);
        }
        return fraction;
    }

    int64_t passAux(const PassJob &j)
    {
        return j.auxStart + j.auxSign * duToSteps(j.aux, j.depthDu * depthFraction(j, j.level));
    }

    // Main-axis shift of a level's start: each level slides along the flank
    // the previous one cut, alternating sides, so one edge cuts at a time.
    int64_t flankOffset(PassJob &j, int32_t level)
    {
        if (j.flankTan == 0 || level <= 1) {
            return 0;
        }
        if (j.offsetLevel != level) {
            double du = 0;
            for (int32_t i = 2; i <= MIN(level, j.levels); i++) {
                du += (i % 2 ? -1 : 1) * (depthFraction(j, i) - depthFraction(j, i - 1)) * j.depthDu * j.flankTan;
            }
            j.offsetSteps = j.cutDirection * duToSteps(j.main, du);
            j.offsetLevel = level;
        }
        return j.offsetSteps;
    }

    int64_t levelStart(PassJob &j, int32_t level)
    {
        return level > totalLevels(j) ? j.mainStart : j.mainStart + flankOffset(j, level);
    }

    int32_t nextLevel(const PassJob &j)
    {
        if (j.level == 0) {
            return 1;
        }
        return j.startIndex + 1 < j.starts ? j.level : j.level + 1 + j.skipLevels;
    }

    int64_t passEnd(const PassJob &j)
    {
        double length = j.lengthDu - j.endReductionDu * (MIN(j.level, j.levels) - 1);
        if (length < 0) {
            length = 0;
        }
        return j.mainStart + j.cutDirection * duToSteps(j.main, length);
    }

    // Aux (X) offset from the taper at a main (Z) position; a growing diameter is +X.
    int64_t taperOffset(const PassJob &j, int64_t mainPos)
    {
        if (j.taper == 0) {
            return 0;
        }
        double towardChuckDu = -stepsToDu(j.main, mainPos - j.mainStart);
        return duToSteps(j.aux, towardChuckDu * (double)j.taper / 2);
    }

    // True once the cut reached the pass end.
    bool cut(PassJob &j, int64_t spindlePos)
    {
        int64_t end = passEnd(j);
        int64_t start = levelStart(j, j.level);
        switch (j.feed)
        {
        case PassFeed::Spindle:
            if (!j.entered) {
                j.spindleFeed.begin(j.main, start, end, spindlePos);
            }
            return j.spindleFeed.tick(spindlePos, overridden(*j.pitchDu));
        case PassFeed::Phased:
        {
            if (!j.entered) {
                int32_t dupr = spindleSign() * signedDu(*j.pitchDu, j.reverse);
                j.link = {j.main, dupr, j.starts, 0, start};
                int64_t startOffset = (int64_t)j.startIndex * countsPerRev() / j.starts;
                j.link.originSpindlePos = helixOrigin(j.link, j.mainStart, startOffset, spindlePos);
            }
            int64_t target = j.link.target(spindlePos);
            if ((target - start) * j.cutDirection < 0) {
                target = start;
            } else if ((target - end) * j.cutDirection > 0) {
                target = end;
            }
            Stepper::moveTo(j.main, target, rapidSpeed(j.main));
            if (j.taper != 0) {
                int64_t aux = passAux(j) + taperOffset(j, Stepper::position(j.main));
                Stepper::moveTo(j.aux, aux, rapidSpeed(j.aux));
            }
            return target == end && Stepper::arrived(j.main) && Stepper::position(j.main) == end;
        }
        case PassFeed::Timed:
            if (*j.speedDuPerMin == 0) {
                Stepper::stop(j.main);
                return false;
            }
            return moveAndWait(j.main, end, duPerMinToStepsPerSec(j.main, overridden(*j.speedDuPerMin)));
        }
        return false;
    }

    // After a pass: next start, else next depth level. False when done.
    bool advance(PassJob &j)
    {
        if (j.level == 0) {
            j.level = 1;
            return true;
        }
        if (++j.startIndex < j.starts) {
            return true;
        }
        j.startIndex = 0;
        j.level += 1 + j.skipLevels;
        j.skipLevels = 0;
        return j.level <= totalLevels(j);
    }
}

namespace Mode::detail
{
    const char *passBegin(PassJob &j, int64_t spindlePos)
    {
        const char *refusal = claim(j.main);
        if (!refusal) {
            refusal = claim(j.aux);
        }
        if (refusal) {
            return refusal;
        }
        j.cutDirection = feedDirection(signedDu(1, j.reverse));
        j.mainStart = Stepper::position(j.main);
        j.auxStart = Stepper::position(j.aux);
        j.level = 0;
        j.startIndex = 0;
        j.skipLevels = 0;
        j.offsetLevel = -1;
        enter(j, Phase::Return);
        return nullptr;
    }

    void passTick(PassJob &j, int64_t spindlePos)
    {
        switch ((Phase)j.phase)
        {
        case Phase::Return:
            if (moveAndWait(j.main, levelStart(j, nextLevel(j)) - j.cutDirection)) {
                enter(j, Phase::Settle);
            }
            return;
        case Phase::Settle:
            if (moveAndWait(j.main, levelStart(j, nextLevel(j)))) {
                enter(j, advance(j) ? Phase::Plunge : Phase::Home);
            }
            return;
        case Phase::Plunge:
            if (moveAndWait(j.aux, passAux(j))) {
                enter(j, Phase::Cut);
            }
            return;
        case Phase::Cut:
        {
            bool done = cut(j, spindlePos);
            j.entered = true;
            if (done) {
                enter(j, Phase::Retract);
            }
            return;
        }
        case Phase::Retract:
        {
            if (!j.entered) {
                int64_t safe = duToSteps(j.aux, Settings::active.safeDistanceDu);
                int64_t from = j.retractToStart ? j.auxStart + taperOffset(j, passEnd(j)) : passAux(j);
                j.retractAux = from - j.auxSign * safe;
                j.entered = true;
            }
            if (moveAndWait(j.aux, j.retractAux)) {
                enter(j, Phase::Return);
            }
            return;
        }
        case Phase::Home:
            if (moveAndWait(j.aux, j.auxStart)) {
                finish();
            }
            return;
        }
    }

    void passSkip(PassJob &j)
    {
        if (j.level > 0 && j.level + j.skipLevels + 1 < totalLevels(j)) {
            j.skipLevels++;
        }
    }

    void passProgress(const PassJob &j, Progress &out)
    {
        static const char *const NAMES[] = {"return", "return", "plunge", "cut", "retract", "home"};
        out.phase = j.level == 0 ? "position" : NAMES[j.phase];
        out.passes = totalLevels(j) * j.starts;
        int32_t pass = j.level == 0 ? 0 : (j.level - 1) * j.starts + j.startIndex + 1;
        out.pass = pass > out.passes ? out.passes : pass;
    }
}
