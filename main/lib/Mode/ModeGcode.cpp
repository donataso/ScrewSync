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
#include "GcodeProgram.hpp"
#include "GcodeStore.hpp"
#include "Events.hpp"
#include "Wcs.hpp"
#include <stdio.h>
#include <string.h>
#include <initializer_list>

// Runs the active G-code program, or one routine call, line by line at
// execution time; ModeSteps runs each step.
namespace
{
    using namespace Mode::Detail;

    constexpr uint32_t LINES_PER_TICK = 64;
    constexpr size_t QUEUE_LEN = 4;

    enum class Phase : uint8_t
    {
        Parse,
        Move,
        Paused, // M0/M1: axes stay claimed until start
    };

    struct Routines : Gcode::RoutineSource
    {
        const char *find(const char *label, size_t &len) override
        {
            return GcodeStore::routine(label, len);
        }
    };

    struct Printer : Gcode::PrintSink
    {
        void print(const char *text) override
        {
            Events::Print msg = {};
            strncpy(msg.text, text, sizeof(msg.text) - 1);
            Events::publish(&PRINT_CHAN, &msg);
        }
    };

    struct Job
    {
        Gcode::Step pending[QUEUE_LEN];
        size_t pendingCount;
        size_t pendingIndex;

        Phase phase;
        bool spindleGated;
        uint32_t executedSteps; // motion steps completed, for display progress
        StepMotion motion;
    };

    uint8_t g_dummyParams[1];
    Gcode::Program g_program;
    Routines g_routines;
    Printer g_printer;
    Job g_job;
    char g_callText[Gcode::MAX_LINE_LEN];
    const char *g_call = nullptr;
    char g_error[96];

    struct QueueSink : Gcode::StepSink
    {
        void emit(const Gcode::Step &step) override
        {
            if (g_job.pendingCount < QUEUE_LEN) {
                g_job.pending[g_job.pendingCount++] = step;
            }
        }
    };

    const char *formatError()
    {
        if (!g_program.errorLine()) {
            snprintf(g_error, sizeof(g_error), "%s", g_program.error());
        } else if (*g_program.errorRoutine()) {
            snprintf(g_error, sizeof(g_error), "%s line %lu: %s", g_program.errorRoutine(),
                     (unsigned long)g_program.errorLine(), g_program.error());
        } else {
            snprintf(g_error, sizeof(g_error), "line %lu: %s", (unsigned long)g_program.errorLine(), g_program.error());
        }
        return g_error;
    }

    const char *claimEnabled()
    {
        bool any = false;
        for (Stepper::Axis axis : {Stepper::Axis::Z, Stepper::Axis::X}) {
            if (!Stepper::isDisabled(axis)) {
                claim(axis);
                any = true;
            }
        }
        return any ? nullptr : "axis disabled";
    }

    const char *startMode(int64_t)
    {
        const char *text;
        size_t len;
        Gcode::Dialect *dialect;
        if (g_call) {
            Gcode::OKind kind;
            char label[Gcode::LABEL_MAX + 1];
            if (!Gcode::scanOword(g_call, kind, label) || kind != Gcode::OKind::Call) {
                return "not a call";
            }
            const char *refusal = claimEnabled();
            if (refusal) {
                return refusal;
            }
            strncpy(g_callText, g_call, sizeof(g_callText) - 1);
            g_callText[sizeof(g_callText) - 1] = '\0';
            text = g_callText;
            len = strlen(g_callText);
            dialect = Gcode::find("linuxcnc");
        } else {
            GcodeStore::ActiveInfo info = GcodeStore::active();
            if (!info.present) {
                return "no program loaded";
            }
            dialect = Gcode::find(info.dialect);
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
            text = GcodeStore::activeText(len);
            if (!text) {
                return "no program loaded";
            }
        }
        g_job = {};
        g_job.spindleGated = g_call == nullptr;
        GcodeStore::resetRoutines();
        g_program.begin({dialect, &params(), &paramSource(), &g_routines, false, &g_printer}, text, len,
                        (int32_t)Wcs::position(Stepper::Axis::Z), (int32_t)Wcs::position(Stepper::Axis::X));
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

    void probeDone()
    {
        int32_t z = (int32_t)Wcs::position(Stepper::Axis::Z);
        int32_t x = (int32_t)Wcs::position(Stepper::Axis::X);
        bool onZ = g_job.motion.mainAxis == Stepper::Axis::Z;
        int32_t tripZ = onZ ? g_job.motion.probeTripDu : z;
        int32_t tripX = onZ ? x : g_job.motion.probeTripDu;
        g_program.probeDone(z, x, tripZ, tripX, g_job.motion.probeFound, g_job.motion.probeMotorDu);
    }

    void tickMode(int64_t spindlePos)
    {
        if (g_job.phase == Phase::Paused) {
            return;
        }
        if (g_job.phase == Phase::Move) {
            const Gcode::Step &s = g_job.pending[g_job.pendingIndex];
            const char *error;
            bool done = g_job.motion.drive(s, spindlePos, g_job.spindleGated, error);
            if (error) {
                finish(error);
            } else if (done) {
                if (s.kind == Gcode::StepKind::Probe) {
                    probeDone();
                }
                g_job.pendingIndex++;
                g_job.executedSteps++;
                g_job.phase = Phase::Parse;
            }
            return;
        }

        if (g_job.pendingIndex >= g_job.pendingCount) {
            g_job.pendingCount = 0;
            g_job.pendingIndex = 0;
            QueueSink sink;
            Gcode::Program::Status status = g_program.next(sink, LINES_PER_TICK);
            if (status == Gcode::Program::Status::Error) {
                finish(formatError());
                return;
            }
            if (status == Gcode::Program::Status::End) {
                finish();
                return;
            }
            if (status == Gcode::Program::Status::Busy) {
                return;
            }
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
        case Gcode::StepKind::SpindleAngle:
            g_job.pendingIndex++;
            applyMachineStep(s);
            break;
        }
    }

    void progressMode(Mode::Progress &out)
    {
        if (g_job.phase == Phase::Paused) {
            out.phase = "paused";
        } else if (*g_program.message()) {
            out.phase = g_program.message();
        } else if (g_job.phase == Phase::Move) {
            out.phase = "move";
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

    void gcodeSetCall(const char *line)
    {
        g_call = line;
    }
}
