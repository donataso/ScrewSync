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
#include "GcodeInterpreter.hpp"
#include "ProbeInput.hpp"
#include "Settings.hpp"
#include "Wcs.hpp"
#include <stddef.h>
#include <zephyr/sys/util.h>

// Touch-off as a fixed G-code sequence on the step runner: G38.2, back
// off, G38.2 at the reprobe feed, back off, G10 L20.
namespace
{
    using namespace Mode::Detail;

    struct Probe
    {
        int32_t probeDstDu;
        int32_t offsetDu;
    };

    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr int32_t REPROBE_MARGIN_DU = 5000;

    Probe g_params = {0, 0};

    const Param PARAMS[] = {
        {"probe-dst", Kind::Du, offsetof(Probe, probeDstDu), 0, MAX_LENGTH_DU, false, true},
        {"offset", Kind::Du, offsetof(Probe, offsetDu), 0, MAX_LENGTH_DU, false, false},
    };

    enum class Line : uint8_t
    {
        Approach,
        BackOff1,
        Reprobe,
        BackOff2,
        SetZero,
        Done,
    };

    // Set by Mode::probeGo(); a plain start is refused.
    Stepper::Axis g_axis = Stepper::Axis::Z;
    bool g_reverse = false;
    bool g_goPending = false;

    Gcode::Interpreter g_interp;
    Line g_next = Line::Done;
    Line g_running = Line::Done;
    bool g_moving = false;
    Gcode::Step g_step;
    StepMotion g_motion;

    struct OneStep : Gcode::StepSink
    {
        Gcode::Step step;
        bool has = false;
        void emit(const Gcode::Step &s) override
        {
            step = s;
            has = true;
        }
    };

    void add(Gcode::Words &w, char letter, double value)
    {
        w.words[w.count++] = {letter, value};
    }

    double mm(double du)
    {
        return du / Gcode::DU_PER_MM;
    }

    void build(Line line, Gcode::Words &w)
    {
        const Settings::Conf &cfg = Settings::g_active;
        char axis = g_axis == Stepper::Axis::Z ? 'Z' : 'X';
        int dir = g_reverse ? 1 : -1;
        double backOff = cfg.probeBackOffDu;
        w.count = 0;
        add(w, 'G', 21);
        switch (line) {
        case Line::Approach:
            add(w, 'G', 91);
            add(w, 'G', 38.2);
            add(w, axis, mm(dir * (double)g_params.probeDstDu));
            add(w, 'F', mm(cfg.probeFeedDuPerMin));
            break;
        case Line::Reprobe:
            add(w, 'G', 91);
            add(w, 'G', 38.2);
            add(w, axis, mm(dir * (backOff + REPROBE_MARGIN_DU)));
            add(w, 'F', mm(cfg.reprobeFeedDuPerMin));
            break;
        case Line::BackOff1:
        case Line::BackOff2:
            add(w, 'G', 91);
            add(w, 'G', 1);
            add(w, axis, mm(-dir * backOff));
            add(w, 'F', mm(cfg.reprobeFeedDuPerMin));
            break;
        case Line::SetZero:
            // The surface is at contact + dir * offset.
            add(w, 'G', 90);
            add(w, 'G', 10);
            add(w, 'L', 20);
            add(w, 'P', 1);
            add(w, axis, mm(-dir * (backOff + g_params.offsetDu)));
            break;
        case Line::Done:
            break;
        }
    }

    const char *startMode(int64_t)
    {
        if (!g_goPending) {
            return "use probe go";
        }
        g_goPending = false;
        if (ProbeInput::contact()) {
            return "contact present";
        }
        const char *refusal = claim(g_axis);
        if (refusal) {
            return refusal;
        }
        g_interp.begin(0, 0);
        g_interp.setMdi(true);
        g_next = Line::Approach;
        g_running = Line::Approach;
        g_moving = false;
        return nullptr;
    }

    void tickMode(int64_t spindlePos)
    {
        if (g_moving) {
            const char *error;
            bool done = g_motion.drive(g_step, spindlePos, false, error);
            if (error) {
                finish(error);
            } else if (done) {
                g_moving = false;
                if (g_step.kind == Gcode::StepKind::Feed && ProbeInput::contact()) {
                    finish("contact not lost");
                }
            }
            return;
        }
        if (g_next == Line::Done) {
            finish();
            return;
        }
        // A G38 move ends where the probe stops it, not at its target.
        g_interp.setPosition((int32_t)Wcs::position(Stepper::Axis::Z), (int32_t)Wcs::position(Stepper::Axis::X));
        Gcode::Words words;
        build(g_next, words);
        OneStep sink;
        const char *err = g_interp.line(words, (uint32_t)g_next + 1, sink);
        if (err) {
            finish(err);
            return;
        }
        g_running = g_next;
        g_next = (Line)((uint8_t)g_next + 1);
        if (!sink.has) {
            return;
        }
        g_step = sink.step;
        if (isMotion(g_step.kind)) {
            g_motion.begin(g_step, spindlePos);
            g_moving = true;
        } else {
            applyMachineStep(g_step);
        }
    }

    void progressMode(Mode::Progress &out)
    {
        switch (g_running) {
        case Line::Approach:
            out.phase = "approach";
            break;
        case Line::BackOff1:
        case Line::BackOff2:
            out.phase = "back off";
            break;
        case Line::Reprobe:
            out.phase = "reprobe";
            break;
        default:
            out.phase = "run";
            break;
        }
    }
}

namespace Mode::Detail
{
    const Ops PROBE = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                       nullptr, nullptr, progressMode, nullptr, nullptr, nullptr};
}

namespace Mode
{
    const char *probeGo(Stepper::Axis axis, bool reverse)
    {
        Detail::Guard guard;
        if (running()) {
            return "already running";
        }
        g_axis = axis;
        g_reverse = reverse;
        g_goPending = true;
        const char *err = select(Id::Probe);
        if (!err) {
            err = start();
        }
        g_goPending = false;
        return err;
    }
}
