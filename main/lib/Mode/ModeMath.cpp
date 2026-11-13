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
#include "ModeMath.hpp"

#include <math.h>
#include <stdlib.h>

namespace Mode::Detail
{
    int32_t feedStepsPerSec(int32_t duPerMin, int32_t motorSteps, int32_t screwPitchDu, int32_t maxSpeed)
    {
        int64_t speed = llround(fabs((double)duPerMin) * motorSteps / screwPitchDu / 60.0);
        if (speed < 1) {
            return 1;
        }
        return speed > maxSpeed ? maxSpeed : (int32_t)speed;
    }

    int32_t vectorFeedStepsPerSec(double vectorDu, int32_t feedDuPerMin, int64_t mainSteps, int32_t cap)
    {
        double timeSec = feedDuPerMin > 0 ? vectorDu / (double)feedDuPerMin * 60.0 : 0;
        double raw = timeSec > 0 ? fabs((double)mainSteps) / timeSec : cap;
        if (raw < 1) {
            return 1;
        }
        return raw > cap ? cap : (int32_t)llround(raw);
    }

    double spindleFeedStepsPerCount(int32_t motorSteps, int32_t pitchDu, int32_t screwPitchDu, int32_t countsPerRev)
    {
        return (double)motorSteps * abs(pitchDu) / ((double)screwPitchDu * countsPerRev);
    }

    int32_t peckPasses(int32_t depthDu, int32_t peckDu)
    {
        return peckDu > 0 ? (depthDu + peckDu - 1) / peckDu : 1;
    }

    int64_t peckDepthDu(int32_t pass, int32_t passes, int32_t peckDu, int32_t depthDu)
    {
        return pass < passes ? (int64_t)peckDu * pass : depthDu;
    }

    void ellipsePoint(double a, double b, bool concave, double angle, double &zDu, double &xDu)
    {
        if (concave) {
            zDu = a * sin(angle);
            xDu = -b * cos(angle);
        } else {
            zDu = a * (1 - cos(angle));
            xDu = -b * (1 - sin(angle));
        }
    }

    double ellipseSpeed(double a, double b, bool concave, double angle)
    {
        double dz = concave ? a * cos(angle) : a * sin(angle);
        double dx = concave ? b * sin(angle) : b * cos(angle);
        double v = sqrt(dz * dz + dx * dx);
        return v < 1 ? 1 : v;
    }
}
