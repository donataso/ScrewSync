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
#include "ModeSteps.hpp"
#include "GcodeDialects.hpp"
#include "GcodeInterpreter.hpp"
#include "GcodeStore.hpp"
#include "Wcs.hpp"
#include <stdio.h>
#include <string.h>

// Walks the active G-code program; ModeSteps runs each step.
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
        Gcode::Dialect *dialect;
        Gcode::Interpreter interp;
        const char *text;
        size_t len;
        size_t pos;
        uint32_t lineNo;

        Gcode::Step pending[2];
        size_t pendingCount;
        size_t pendingIndex;

        Phase phase;
        uint32_t executedSteps; // motion steps completed, for display progress
        StepMotion motion;
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
            const char *err = Gcode::interpretLine(*j.dialect, j.interp, buf, j.lineNo, sink);
            if (err) {
                static char reason[48];
                snprintf(reason, sizeof(reason), "line %lu: %s", (unsigned long)j.lineNo, err);
                return reason;
            }
        }
        return nullptr;
    }

    const char *startMode(int64_t)
    {
        GcodeStore::ActiveInfo info = GcodeStore::active();
        if (!info.present) {
            return "no program loaded";
        }
        Gcode::Dialect *dialect = Gcode::find(info.dialect);
        if (!dialect) {
            return "unknown dialect";
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
        g_job.dialect = dialect;
        g_job.text = text;
        g_job.len = len;
        dialect->begin(g_job.interp, (int32_t)Wcs::position(Stepper::Axis::Z), (int32_t)Wcs::position(Stepper::Axis::X));
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
            const char *error;
            bool done = g_job.motion.drive(s, spindlePos, true, error);
            if (error) {
                finish(error);
            } else if (done) {
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
        case Gcode::StepKind::Probe:
            g_job.motion.begin(s, spindlePos);
            g_job.phase = Phase::Move;
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
        case Gcode::StepKind::SetPosition:
        case Gcode::StepKind::Motors:
        case Gcode::StepKind::SpindleAngle:
            g_job.pendingIndex++;
            applyMachineStep(s);
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
