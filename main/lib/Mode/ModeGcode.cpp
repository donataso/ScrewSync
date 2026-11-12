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
#include "ModeInternal.hpp"
#include "GcodeFlavors.hpp"
#include "GcodeStore.hpp"
#include "Rpm.hpp"
#include <math.h>
#include <stdlib.h>
#include <zephyr/sys/util.h>
#include <stdio.h>
#include <string.h>

// Walks the active G-code program. Z/X are driven together: the axis with
// the larger du delta is "main" and moves at the computed speed (rapid for
// G0, feed-derived for G1, phased to the thread helix for G32); "aux"
// chases main's fraction of progress, same pattern as a taper's X-follows-Z
// elsewhere. Positions are absolute from the machine zero, no flavor offset.
namespace
{
    using namespace Mode::Detail;

    enum class Phase : uint8_t
    {
        Parse,
        Move,
        Paused, // M0/M1: axes stay claimed until start
    };

    struct Job
    {
        Gcode::Flavor *flavor;
        const char *text;
        size_t len;
        size_t pos;
        uint32_t lineNo;

        Gcode::Step pending[2];
        size_t pendingCount;
        size_t pendingIndex;

        Phase phase;
        uint32_t executedSteps; // motion steps completed, for display progress
        Stepper::Axis mainAxis;
        Stepper::Axis auxAxis;
        int64_t mainFrom, mainTo, auxFrom, auxTo;
        double vectorDu; // Feed only: path length for the per-minute speed split
        Sync::Link thread;
    };

    uint8_t g_dummyParams[1];
    Job g_job;

    struct QueueSink : Gcode::StepSink
    {
        Job *job;
        void emit(const Gcode::Step &step) override
        {
            if (job->pendingCount < 2) {
                job->pending[job->pendingCount++] = step;
            }
        }
    };

    // Parses lines until a step is queued or the text runs out; pendingCount
    // stays 0 at end of text (an implicit end, no M2/M30 required).
    const char *fillPending(Job &j)
    {
        while (j.pendingIndex >= j.pendingCount && j.pos < j.len) {
            j.pendingCount = 0;
            j.pendingIndex = 0;
            size_t lineStart = j.pos;
            size_t lineEnd = lineStart;
            while (lineEnd < j.len && j.text[lineEnd] != '\n') {
                lineEnd++;
            }
            size_t copyLen = lineEnd - lineStart;
            if (copyLen >= Gcode::MAX_LINE_LEN) {
                copyLen = Gcode::MAX_LINE_LEN - 1;
            }
            char buf[Gcode::MAX_LINE_LEN];
            memcpy(buf, j.text + lineStart, copyLen);
            buf[copyLen] = '\0';
            j.pos = lineEnd < j.len ? lineEnd + 1 : lineEnd;
            j.lineNo++;

            QueueSink sink;
            sink.job = &j;
            const char *err = Gcode::interpretLine(*j.flavor, buf, j.lineNo, sink);
            if (err) {
                static char reason[48];
                snprintf(reason, sizeof(reason), "line %lu: %s", (unsigned long)j.lineNo, err);
                return reason;
            }
        }
        return nullptr;
    }

    double axisDeltaDu(Stepper::Axis axis, int32_t targetDu)
    {
        return (double)targetDu - stepsToDu(axis, Stepper::position(axis));
    }

    void beginStep(Job &j, const Gcode::Step &s, int64_t spindlePos)
    {
        double dz = axisDeltaDu(Stepper::Axis::Z, s.zDu);
        double dx = axisDeltaDu(Stepper::Axis::X, s.xDu);
        j.mainAxis = fabs(dz) >= fabs(dx) ? Stepper::Axis::Z : Stepper::Axis::X;
        j.auxAxis = j.mainAxis == Stepper::Axis::Z ? Stepper::Axis::X : Stepper::Axis::Z;
        j.mainFrom = Stepper::position(j.mainAxis);
        j.auxFrom = Stepper::position(j.auxAxis);
        j.mainTo = duToSteps(j.mainAxis, j.mainAxis == Stepper::Axis::Z ? s.zDu : s.xDu);
        j.auxTo = duToSteps(j.auxAxis, j.auxAxis == Stepper::Axis::Z ? s.zDu : s.xDu);
        // A sub-step du delta can round the chosen main axis to zero steps;
        // fall back to whichever axis actually has somewhere to go.
        if (j.mainTo == j.mainFrom && j.auxTo != j.auxFrom) {
            Stepper::Axis axis = j.mainAxis;
            j.mainAxis = j.auxAxis;
            j.auxAxis = axis;
            int64_t v = j.mainFrom;
            j.mainFrom = j.auxFrom;
            j.auxFrom = v;
            v = j.mainTo;
            j.mainTo = j.auxTo;
            j.auxTo = v;
        }
        if (s.kind == Gcode::StepKind::Thread) {
            int32_t dupr = (j.mainTo > j.mainFrom ? -1 : 1) * abs(s.feed);
            j.thread = {j.mainAxis, dupr, 1, 0, j.mainFrom};
            j.thread.originSpindlePos = helixOrigin(j.thread, j.mainFrom, 0, spindlePos);
        } else if (s.kind == Gcode::StepKind::Feed) {
            j.vectorDu = sqrt(dz * dz + dx * dx);
        }
        j.phase = Phase::Move;
    }

    // True once aux reaches its target; finishes the job on a travel clamp.
    bool followAux(Job &j)
    {
        if (j.auxTo == j.auxFrom) {
            return true;
        }
        double fraction = (double)(Stepper::position(j.mainAxis) - j.mainFrom) / (double)(j.mainTo - j.mainFrom);
        int64_t target = j.auxFrom + (int64_t)llround(fraction * (double)(j.auxTo - j.auxFrom));
        if (Stepper::moveTo(j.auxAxis, target, rapidSpeed(j.auxAxis)) != target) {
            finish(Stepper::isDisabled(j.auxAxis) ? "axis disabled" : "travel limit");
            return false;
        }
        return target == j.auxTo && Stepper::arrived(j.auxAxis) && Stepper::position(j.auxAxis) == j.auxTo;
    }

    // True once the step's motion is complete. Freezes (no finish, no
    // progress) while the spindle isn't turning, like the reference
    // firmware's "spindle pauses gcode" behaviour.
    bool driveStep(Job &j, int64_t spindlePos, const Gcode::Step &s)
    {
        if (s.kind == Gcode::StepKind::Thread) {
            int64_t target = j.thread.target(spindlePos);
            int64_t lo = MIN(j.mainFrom, j.mainTo);
            int64_t hi = MAX(j.mainFrom, j.mainTo);
            target = CLAMP(target, lo, hi);
            if (Stepper::moveTo(j.mainAxis, target, rapidSpeed(j.mainAxis)) != target) {
                finish(Stepper::isDisabled(j.mainAxis) ? "axis disabled" : "travel limit");
                return false;
            }
            bool mainDone = target == j.mainTo && Stepper::arrived(j.mainAxis) && Stepper::position(j.mainAxis) == j.mainTo;
            bool auxDone = followAux(j);
            return mainDone && auxDone;
        }
        if (Rpm::value() == 0) {
            Stepper::stop(j.mainAxis);
            Stepper::stop(j.auxAxis);
            return false;
        }
        int32_t cap = rapidSpeed(j.mainAxis);
        int32_t speed;
        if (s.kind == Gcode::StepKind::Rapid) {
            speed = cap;
        } else {
            int32_t feed = overridden(s.feed);
            double timeSec = feed > 0 ? j.vectorDu / (double)feed * 60.0 : 0;
            double raw = timeSec > 0 ? fabs((double)(j.mainTo - j.mainFrom)) / timeSec : cap;
            if (raw < 1) {
                speed = 1;
            } else if (raw > cap) {
                speed = cap;
            } else {
                speed = (int32_t)llround(raw);
            }
        }
        if (Stepper::moveTo(j.mainAxis, j.mainTo, speed) != j.mainTo) {
            finish(Stepper::isDisabled(j.mainAxis) ? "axis disabled" : "travel limit");
            return false;
        }
        bool mainDone = Stepper::arrived(j.mainAxis) && Stepper::position(j.mainAxis) == j.mainTo;
        bool auxDone = followAux(j);
        return mainDone && auxDone;
    }

    const char *startMode(int64_t)
    {
        GcodeStore::ActiveInfo info = GcodeStore::active();
        if (!info.present) {
            return "no program loaded";
        }
        Gcode::Flavor *flavor = Gcode::find(info.flavor);
        if (!flavor) {
            return "unknown flavor";
        }
        const char *refusal = claim(Stepper::Axis::Z);
        if (!refusal) {
            refusal = claim(Stepper::Axis::X);
        }
        if (refusal) {
            return refusal;
        }

        size_t len;
        const char *text = GcodeStore::activeText(len);
        if (!text) {
            return "no program loaded";
        }
        g_job = {};
        g_job.flavor = flavor;
        g_job.text = text;
        g_job.len = len;
        flavor->begin((int32_t)llround(stepsToDu(Stepper::Axis::Z, Stepper::position(Stepper::Axis::Z))),
                      (int32_t)llround(stepsToDu(Stepper::Axis::X, Stepper::position(Stepper::Axis::X))));
        return nullptr;
    }

    bool resumeMode()
    {
        if (g_job.phase != Phase::Paused) {
            return false;
        }
        g_job.phase = Phase::Parse;
        return true;
    }

    void tickMode(int64_t spindlePos)
    {
        if (g_job.phase == Phase::Paused) {
            return;
        }
        if (g_job.phase == Phase::Move) {
            const Gcode::Step &s = g_job.pending[g_job.pendingIndex];
            if (driveStep(g_job, spindlePos, s)) {
                g_job.pendingIndex++;
                g_job.executedSteps++;
                g_job.phase = Phase::Parse;
            }
            return;
        }

        const char *err = fillPending(g_job);
        if (err) {
            finish(err);
            return;
        }
        if (g_job.pendingIndex >= g_job.pendingCount) {
            finish();
            return;
        }

        const Gcode::Step &s = g_job.pending[g_job.pendingIndex];
        switch (s.kind) {
        case Gcode::StepKind::Rapid:
        case Gcode::StepKind::Feed:
        case Gcode::StepKind::Thread:
            beginStep(g_job, s, spindlePos);
            break;
        case Gcode::StepKind::Pause:
        case Gcode::StepKind::OptionalPause:
            g_job.pendingIndex++;
            g_job.phase = Phase::Paused;
            break;
        case Gcode::StepKind::End:
            g_job.pendingIndex++;
            finish();
            break;
        }
    }

    void progressMode(Mode::Progress &out)
    {
        if (g_job.phase == Phase::Move) {
            out.phase = "move";
        } else if (g_job.phase == Phase::Paused) {
            out.phase = "paused";
        } else {
            out.phase = "line";
        }
        out.waiting = g_job.phase == Phase::Paused;
        out.step = g_job.executedSteps;
    }
}

namespace Mode::Detail
{
    const Ops GCODE = {true, g_dummyParams, sizeof(g_dummyParams), nullptr, 0, startMode, tickMode,
                       nullptr, nullptr, progressMode, nullptr, nullptr, resumeMode};
}
