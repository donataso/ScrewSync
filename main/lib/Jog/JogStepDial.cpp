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
#include "JogStepDial.hpp"
#include "Handwheel.hpp"
#include "JogStep.hpp"
#include "RunSettings.hpp"
#include <stdlib.h>

namespace
{
    // Flash is written once the encoder stops.
    constexpr uint32_t PERSIST_DELAY_MS = 1000;

    bool g_started = false;
    int64_t g_lastCount = 0;
    uint32_t g_lastMoveMs = 0;
    bool g_dirty = false;
}

namespace JogStepDial
{
    void tick(uint32_t nowMs)
    {
        int64_t count = Handwheel::jogStep();
        if (!g_started) {
            g_started = true;
            g_lastCount = count;
            return;
        }
        int64_t delta = count - g_lastCount;
        if (delta == 0) {
            if (g_dirty && nowMs - g_lastMoveMs >= PERSIST_DELAY_MS) {
                RunSettings::persistJogStep();
                g_dirty = false;
            }
            return;
        }
        g_lastCount = count;
        uint32_t msPerDetent = (nowMs - g_lastMoveMs) / (uint32_t)llabs(delta);
        g_lastMoveMs = nowMs;
        bool inch = RunSettings::units() == RunSettings::Units::Inch;
        int32_t du = Jog::dialStep(RunSettings::jogStepDu(), (int32_t)delta, Jog::dialMultiplier(msPerDetent), inch);
        RunSettings::setJogStepDu(du, false);
        g_dirty = true;
    }
}
