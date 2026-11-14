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
#include "DisplayInternal.hpp"
#include "GcodeCore.hpp"
#include "GcodeStore.hpp"
#include "JogStep.hpp"
#include "Machine.hpp"
#include "Motion.hpp"
#include "Wifi.hpp"
#include "display_config.hpp"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

using namespace Display::Detail;
using namespace Display::Theme;
using namespace Nextion;

LOG_MODULE_DECLARE(display);

namespace
{
    uint32_t g_lastPushMs = 0;

    // RAM files are lost when the display restarts.
    atomic_t g_previewLoaded = ATOMIC_INIT(0);
    Display::PreviewMapping g_previewMapping;

    void pushAxis(char letter, Stepper::Axis axis, const Status::Axis &a)
    {
        bool z = letter == 'z';
        int on = z ? COLOR_Z : COLOR_X;
        int onPressed = z ? COLOR_Z_PRESSED : COLOR_X_PRESSED;
        char key[KEY_MAX];
        char text[24];

        snprintf(key, sizeof(key), "%cOn.val", letter);
        pushNum(key, a.enabled);

        formatLength(Motion::stepsToDu(axis, a.pos), text, sizeof(text));
        snprintf(key, sizeof(key), "%cPos.txt", letter);
        pushText(key, "%s", text);
        snprintf(key, sizeof(key), "%cPos", letter);
        pushColor(key, "pco", a.enabled ? COLOR_TEXT : COLOR_DIM);

        snprintf(key, sizeof(key), "%cZero", letter);
        pushColor(key, "bco", a.enabled ? COLOR_ZERO : COLOR_AXIS_OFF);
        pushColor(key, "bco2", a.enabled ? COLOR_ZERO_PRESSED : COLOR_AXIS_OFF);
        pushColor(key, "pco", a.enabled ? COLOR_TEXT : COLOR_DIM);

        static const char *const SUFFIXES[] = {"Enable", "Minus", "Plus"};
        static const char *const PROBE_Z[] = {"probeLeft", "probeRight"};
        static const char *const PROBE_X[] = {"probeUp", "probeDown"};
        auto pushAxisButton = [&](const char *name) {
            pushColor(name, "bco", a.enabled ? on : COLOR_AXIS_OFF);
            pushColor(name, "bco2", a.enabled ? onPressed : COLOR_AXIS_OFF);
            pushColor(name, "pco", a.enabled ? COLOR_TEXT : COLOR_DIM);
        };
        for (const char *suffix : SUFFIXES) {
            snprintf(key, sizeof(key), "%c%s", letter, suffix);
            pushAxisButton(key);
        }
        for (const char *name : z ? PROBE_Z : PROBE_X) {
            pushAxisButton(name);
        }
    }

    void pushJogStep(int32_t stepDu)
    {
        const int32_t *presets = Jog::stepPresets(inchUnits());
        bool custom = true;
        char key[KEY_MAX];
        char label[24];
        for (size_t i = 0; i < Jog::STEP_PRESET_COUNT; i++) {
            bool selected = presets[i] == stepDu;
            custom = custom && !selected;
            snprintf(key, sizeof(key), "step%u", (unsigned)i);
            pushSelected(key, selected);
            formatLength(presets[i], label, sizeof(label));
            trimZeros(label);
            snprintf(key, sizeof(key), "step%u.txt", (unsigned)i);
            pushText(key, "%s", label);
            snprintf(key, sizeof(key), "stepDu%u.val", (unsigned)i);
            pushNum(key, presets[i]);
        }
        pushSelected("stepCustom", custom);
        formatLength(stepDu, label, sizeof(label));
        trimZeros(label);
        pushText("stepCustom.txt", "%s", custom ? label : "...");
    }

    // Returns whether the run state or the skip layout (narrow stop + skip) toggled.
    bool pushRun(Mode::Id current, bool running, bool ready, const Mode::Progress &job)
    {
        bool active = running && !job.waiting;
        bool runChanged = pushNum("running.val", active);
        pushNum("ready.val", ready);
        pushNum("needZero.val", (int)Mode::startPrompt());
        bool passCount = active && job.passes > 1;
        bool canSkip = passCount && Mode::canSkip(current);
        if (canSkip) {
            pushText("stopPass.txt", "STOP %d/%d", (int)job.pass, (int)job.passes);
        } else if (passCount) {
            pushText("startStop.txt", "STOP %d/%d", (int)job.pass, (int)job.passes);
        } else {
            pushText("startStop.txt", active ? "STOP" : "START");
        }
        int startStopColor = COLOR_AXIS_OFF;
        if (active) {
            startStopColor = COLOR_STOP;
        } else if (ready) {
            startStopColor = COLOR_START;
        }
        pushColor("startStop", "bco", startStopColor);
        pushColor("startStop", "pco", active || ready ? COLOR_TEXT : COLOR_DIM);
        bool skipChanged = pushNum("canSkip.val", canSkip);
        return runChanged || skipChanged;
    }

    // Returns whether the override row's visibility toggled.
    bool pushOverride(Mode::Id current, bool running, int32_t pct)
    {
        bool shown = Mode::hasFeedOverride(current);
        if (shown) {
            pushText("ovrMinus.txt", "-%d%%", (int)Mode::OVERRIDE_STEP_PCT);
            pushText("ovrPlus.txt", "+%d%%", (int)Mode::OVERRIDE_STEP_PCT);
            pushText("ovr.txt", "%d%%", (int)pct);
            bool changed = pct != 100;
            pushColor("ovr", "bco", changed ? COLOR_ZERO : COLOR_BUTTON);
            pushColor("ovr", "bco2", changed ? COLOR_ZERO_PRESSED : COLOR_BUTTON_PRESSED);
            pushColor("ovr", "pco", running ? COLOR_TEXT : COLOR_DIM);
            pushColor("ovrMinus", "pco", running && pct > Mode::OVERRIDE_MIN_PCT ? COLOR_TEXT : COLOR_DIM);
            pushColor("ovrPlus", "pco", running && pct < Mode::OVERRIDE_MAX_PCT ? COLOR_TEXT : COLOR_DIM);
        }
        return pushNum("ovrOn.val", shown);
    }

    int previewPx(int origin, int span, float imagePx)
    {
        int px = origin + (int)lroundf(imagePx) - TOOL_SIZE / 2;
        return CLAMP(px, origin, origin + span - TOOL_SIZE);
    }

    // Returns whether the tool marker's visibility toggled.
    bool pushTool(const Status::Snapshot &s)
    {
        Display::PreviewMapping m = g_previewMapping;
        bool shown = atomic_get(&g_previewLoaded) && m.pxPerMm > 0;
        if (shown) {
            float zMm = (float)(Motion::stepsToDu(Stepper::Axis::Z, s.z.pos) / Gcode::DU_PER_MM);
            float xMm = (float)(Motion::stepsToDu(Stepper::Axis::X, s.x.pos) / Gcode::DU_PER_MM);
            pushNum("gcodeTool.x", previewPx(PREVIEW_X, PREVIEW_W, m.originX + zMm * m.pxPerMm));
            pushNum("gcodeTool.y", previewPx(PREVIEW_Y, PREVIEW_H, m.originY + xMm * m.pxPerMm));
        }
        return pushNum("gcodeToolOn.val", shown);
    }

    // Program name, progress and pause state; returns whether a visibility flag changed.
    bool pushGcode(Mode::Id current, const Status::Snapshot &s)
    {
        bool changed = pushNum("gcodeOn.val", current == Mode::Id::Gcode);
        if (current != Mode::Id::Gcode) {
            return changed;
        }
        GcodeStore::ActiveInfo info = GcodeStore::active();
        changed |= pushNum("gcodeLoaded.val", info.present);
        if (info.present) {
            changed |= pushTool(s);
            pushText("gcodeName.txt", "%s", info.name);
            pushText("gcodeProgress.txt", "%lu/%lu steps", (unsigned long)s.job.step, (unsigned long)info.steps);
        } else {
            const char *ip = Wifi::ip();
            pushText("gcodeEmpty.txt", "Load from web: %s", ip && *ip ? ip : "-");
        }
        changed |= pushNum("gcodePaused.val", s.job.waiting);
        return changed;
    }

    bool pushProbe(Mode::Id current, const Status::Snapshot &s)
    {
        bool changed = pushNum("probeOn.val", current == Mode::Id::Probe);
        if (current != Mode::Id::Probe) {
            return changed;
        }
        const char *phase;
        if (s.running) {
            phase = s.job.phase;
        } else if (*s.error) {
            phase = s.error;
        } else {
            phase = "ready";
        }
        pushText("probePhase.txt", "%s", phase);
        pushColor("probeLight", "bco", s.probeContact ? COLOR_START : COLOR_AXIS_OFF);
        return changed;
    }

    void pushMode(const Status::Snapshot &s)
    {
        Mode::Id current = Mode::current();
        char label[24];
        snprintf(label, sizeof(label), "%s", ModeCells::label(current));
        for (char *c = label; *c; c++) {
            *c = (char)toupper((unsigned char)*c);
        }
        if (pushText("modeName.txt", "%s", label)) {
            writeCommand("click modeSync,1");
        }
        pushMenu(current);
        bool layoutChanged = pushPanel(current, s.params);
        layoutChanged |= pushRun(current, s.running, s.ready, s.job);
        layoutChanged |= pushOverride(current, s.running, s.feedOverridePct);
        layoutChanged |= pushNum("picOn.val", current == Mode::Id::Gcode && atomic_get(&g_previewLoaded));
        layoutChanged |= pushGcode(current, s);
        layoutChanged |= pushProbe(current, s);
        if (layoutChanged) {
            writeCommand("click layoutSync,1");
        }
    }

    void pushJobError(const char *error, bool running)
    {
        static const char *shown = "";
        if (running) {
            shown = "";
        } else if (error != shown) {
            shown = error;
            if (*error) {
                showToast(error);
            }
        }
    }

    void pushEstop()
    {
        bool tripped = Machine::state() == Machine::State::EStop;
        if (!pushNum("estop.val", tripped)) {
            return;
        }
        if (tripped) {
            showToast("EStop tripped");
        } else {
            writeCommand("click toastHide,1");
        }
    }

    void pushWifi()
    {
        Wifi::Mode m = Wifi::mode();
        pushSelected("wifiOn", Wifi::enabled());
        pushSelected("wifiOff", !Wifi::enabled());
        pushSelected("apOn", m == Wifi::Mode::AccessPoint);
        pushSelected("apOff", m != Wifi::Mode::AccessPoint);
        switch (m) {
        case Wifi::Mode::Station:
            pushText("wifiState.txt", "%s", Wifi::ssid());
            break;
        case Wifi::Mode::AccessPoint:
            pushText("wifiState.txt", "AP %s", Wifi::apName());
            break;
        default:
            pushText("wifiState.txt", "%s", Wifi::modeName());
            break;
        }
        const char *ip = Wifi::ip();
        pushText("wifiIp.txt", "%s", ip && *ip ? ip : "-");
    }
}

namespace Display::Detail
{
    void pushStatus()
    {
        uint32_t now = k_uptime_get_32();
        if (now - g_lastPushMs < DISPLAY_UPDATE_INTERVAL_MS) {
            return;
        }
        g_lastPushMs = now;
        if (page() == PAGE_THREADS) {
            pushPicker();
            return;
        }

        Status::Snapshot s = Status::get();
        pushText("rpm.txt", "%d RPM", (int)s.rpm);
        pushText("angle.txt", "%.1f\xC2\xB0", (double)s.angleDeg);
        pushNum("unitsInch.val", inchUnits());
        pushSelected("unitMm", !inchUnits());
        pushSelected("unitIn", inchUnits());
        pushAxis('z', Stepper::Axis::Z, s.z);
        pushAxis('x', Stepper::Axis::X, s.x);
        pushJogStep(s.jogStepDu);
        pushMode(s);
        pushJobError(s.error, s.running);
        pushEstop();
        pushWifi();
    }

    void pushSoon()
    {
        g_lastPushMs = 0;
    }

    void clearPreview()
    {
        atomic_set(&g_previewLoaded, 0);
    }
}

namespace Display
{
    bool sendPreview(const uint8_t *jpeg, size_t len, const PreviewMapping &mapping)
    {
        if (!Nextion::pause()) {
            return false;
        }
        bool ok = Nextion::twfile(PREVIEW_PATH, jpeg, len);
        if (ok) {
            // Re-setting the path redraws a picture already showing.
            writeRaw("gcodePic.path=\"\"");
            char cmd[48];
            snprintf(cmd, sizeof(cmd), "gcodePic.path=\"%s\"", PREVIEW_PATH);
            writeRaw(cmd);
        }
        g_previewMapping = mapping;
        atomic_set(&g_previewLoaded, ok);
        Nextion::release();
        LOG_INF("preview %s, %u bytes", ok ? "sent" : "failed", (unsigned)len);
        return ok;
    }
}
