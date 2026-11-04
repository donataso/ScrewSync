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
#include "Sync.hpp"
#include "SyncRatio.hpp"
#include "Settings.hpp"

namespace
{
    const Settings::Axis &cfgOf(Stepper::Axis axis)
    {
        return axis == Stepper::Axis::Z ? Settings::active.z : Settings::active.x;
    }

    int32_t countsPerRev() { return Settings::active.encoderPpr * 2; }
}

namespace Sync
{
    void Link::engage(int64_t spindlePos)
    {
        originSpindlePos = spindlePos;
        originAxisPos = Stepper::position(axis);
    }

    int64_t Link::target(int64_t spindlePos) const
    {
        const Settings::Axis &cfg = cfgOf(axis);
        // Forward rotation feeds a positive dupr toward -axis.
        return ratioTarget(originAxisPos, originSpindlePos, spindlePos, cfg.motorSteps, -duprDu, starts,
                           cfg.screwPitchDu, countsPerRev());
    }

    double Link::stepsPerCount() const
    {
        const Settings::Axis &cfg = cfgOf(axis);
        return ratio(cfg.motorSteps, -duprDu, starts, cfg.screwPitchDu, countsPerRev());
    }

    void Link::setDupr(int32_t newDuprDu, int64_t spindlePos)
    {
        duprDu = newDuprDu;
        engage(spindlePos);
    }
}
