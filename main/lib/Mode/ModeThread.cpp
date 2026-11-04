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
#include "ThreadStore.hpp"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/sys/util.h>

namespace
{
    using namespace Mode::detail;

    // Along Z, X steps deeper each pass (deeper first passes).
    struct Thread
    {
        int32_t pitchDu;
        bool leftHand;
        int32_t lengthDu;
        int32_t depthDu;
        int32_t passes;
        int32_t starts;
        bool internal;
        float taper; // + = diameter grows toward the chuck
        bool spindleReverse;
        bool flank;  // alternating flank infeed
        bool spring; // extra pass at full depth
        int32_t flankAngle; // included, degrees; set by the preset
        char group[ThreadTable::GROUP_NAME_MAX];
        char preset[ThreadTable::DESIGNATION_MAX]; // "" = custom
    };

    constexpr double MAX_PITCH_DU = 1000000;
    constexpr double MAX_LENGTH_DU = 10000000;
    constexpr double MAX_PASSES = 1000;
    constexpr double MAX_STARTS = 16;
    constexpr double MAX_FLANK_ANGLE = ThreadTable::MAX_FLANK_ANGLE;
    constexpr int32_t DEFAULT_FLANK_ANGLE = 60;
    // Infeed a little steeper than the flank, so the trailing edge skims instead of rubbing.
    constexpr double FLANK_RELIEF_DEG = 3;
    constexpr double RAD_PER_DEG = 3.14159265358979323846 / 180;

    Thread g_params = {0, false, 0, 0, 5, 1, false, 0, false, false, false, DEFAULT_FLANK_ANGLE, "", ""};

    const Param PARAMS[] = {
        {"pitch", Kind::Du, offsetof(Thread, pitchDu), 0, MAX_PITCH_DU, false, true},
        {"hand", Kind::Flag, offsetof(Thread, leftHand), 0, 0, false, false, "rh|lh"},
        {"length", Kind::Du, offsetof(Thread, lengthDu), 0, MAX_LENGTH_DU, false, true},
        {"depth", Kind::Du, offsetof(Thread, depthDu), 0, MAX_LENGTH_DU, false, true},
        {"passes", Kind::Count, offsetof(Thread, passes), 1, MAX_PASSES, false},
        {"starts", Kind::Count, offsetof(Thread, starts), 1, MAX_STARTS, false},
        {"internal", Kind::Flag, offsetof(Thread, internal), 0, 0, false},
        {"taper", Kind::Angle, offsetof(Thread, taper), -1, 1, false},
        {"spindle", Kind::Flag, offsetof(Thread, spindleReverse), 0, 0, false, false, "fwd|rev"},
        {"flank", Kind::Flag, offsetof(Thread, flank), 0, 0, false},
        {"spring", Kind::Flag, offsetof(Thread, spring), 0, 0, false},
        {"angle", Kind::Count, offsetof(Thread, flankAngle), 1, MAX_FLANK_ANGLE - 1, false},
        {"group", Kind::Text, offsetof(Thread, group), 0, ThreadTable::GROUP_NAME_MAX, false},
        {"preset", Kind::Text, offsetof(Thread, preset), 0, ThreadTable::DESIGNATION_MAX, false},
    };

    PassJob g_job;

    // Internal flips the taper.
    bool applyPreset()
    {
        ThreadStore::Preset p;
        if (!ThreadStore::lookup(g_params.group, g_params.preset, p)) {
            return false;
        }
        g_params.pitchDu = p.pitchDu;
        g_params.depthDu = g_params.internal ? p.depthInternalDu : p.depthExternalDu;
        g_params.taper = g_params.internal ? -p.taper : p.taper;
        g_params.flankAngle = (int32_t)lroundf(p.flankAngle);
        return true;
    }

    void onChange(const Param &param)
    {
        if (!g_params.preset[0]) {
            return;
        }
        if (!strcmp(param.name, "internal")) {
            if (!applyPreset()) {
                g_params.group[0] = g_params.preset[0] = '\0';
            }
        } else if (!strcmp(param.name, "pitch") || !strcmp(param.name, "depth") || !strcmp(param.name, "taper")) {
            g_params.group[0] = g_params.preset[0] = '\0';
        }
    }

    const char *startMode(int64_t spindlePos)
    {
        g_job = {};
        g_job.main = Stepper::Axis::Z;
        g_job.aux = Stepper::Axis::X;
        g_job.auxSign = g_params.internal ? 1 : -1;
        g_job.lengthDu = g_params.lengthDu;
        g_job.depthDu = g_params.depthDu;
        g_job.levels = g_params.passes;
        g_job.starts = g_params.starts;
        g_job.squaredDepth = true;
        g_job.retractToStart = true;
        g_job.feed = PassFeed::Phased;
        g_job.pitchDu = &g_params.pitchDu;
        // Reverse rotation cuts the same helix feeding the other way.
        g_job.reverse = g_params.leftHand != g_params.spindleReverse;
        g_job.taper = g_params.taper;
        if (g_params.flank) {
            double halfAngle = g_params.flankAngle / 2.0 - FLANK_RELIEF_DEG;
            g_job.flankTan = halfAngle > 0 ? tan(halfAngle * RAD_PER_DEG) : 0;
        }
        g_job.finishLevels = g_params.spring ? 1 : 0;
        return passBegin(g_job, spindlePos);
    }

    void tickMode(int64_t spindlePos) { passTick(g_job, spindlePos); }
    void progressMode(Mode::Progress &out) { passProgress(g_job, out); }
    void skipMode() { passSkip(g_job); }
}

namespace Mode::detail
{
    const Ops THREAD = {true, &g_params, sizeof(g_params), PARAMS, ARRAY_SIZE(PARAMS), startMode, tickMode,
                        nullptr, onChange, progressMode, skipMode, &g_params.spindleReverse};
}

namespace Mode
{
    const char *threadPreset(const char *group, const char *designation)
    {
        detail::Guard guard;
        if (detail::isRunning(Id::Thread)) {
            return "stop first";
        }
        Thread previous = g_params;
        if (strlen(group) >= sizeof(g_params.group) || strlen(designation) >= sizeof(g_params.preset)) {
            return "unknown thread";
        }
        strcpy(g_params.group, group);
        strcpy(g_params.preset, designation);
        if (!applyPreset()) {
            g_params = previous;
            return "unknown thread";
        }
        detail::saveParams(Id::Thread);
        return nullptr;
    }
}
