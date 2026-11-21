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
#include "ModePassPlan.hpp"
#include <math.h>

namespace
{
    constexpr double SANDVIK_FIRST_PASS_AREA = 0.3;

    int32_t depthLevel(const Mode::Detail::PassPlan &plan, int32_t level)
    {
        return level < plan.levels ? level : plan.levels;
    }

    double linearFraction(int32_t level, int32_t levels)
    {
        return (double)level / levels;
    }

    double squaredFraction(int32_t level, int32_t levels)
    {
        double remaining = 1 - linearFraction(level, levels);
        return 1 - remaining * remaining;
    }

    double sandvikFraction(int32_t level, int32_t levels)
    {
        if (level == 0) {
            return 0;
        }
        if (levels == 1) {
            return 1;
        }
        double area = level == 1 ? SANDVIK_FIRST_PASS_AREA : level - 1;
        return sqrt(area / (levels - 1));
    }
}

namespace Mode::Detail
{
    int32_t passTotalLevels(const PassPlan &plan)
    {
        return plan.levels + plan.finishLevels;
    }

    double passDepthFraction(const PassPlan &plan, int32_t level)
    {
        int32_t depth = depthLevel(plan, level);
        switch (plan.depthProfile) {
        case DepthProfile::Squared:
            return squaredFraction(depth, plan.levels);
        case DepthProfile::Sandvik:
            return sandvikFraction(depth, plan.levels);
        case DepthProfile::Linear:
            break;
        }
        return linearFraction(depth, plan.levels);
    }

    double passDepthDu(const PassPlan &plan, int32_t level)
    {
        return plan.depthDu * passDepthFraction(plan, level);
    }

    double passFlankOffsetDu(const PassPlan &plan, int32_t level)
    {
        if (plan.flank == FlankInfeed::Off || level <= 1) {
            return 0;
        }
        double du = 0;
        for (int32_t i = 2; i <= depthLevel(plan, level); i++) {
            int side = plan.flank == FlankInfeed::OneSide || i % 2 == 0 ? 1 : -1;
            du += side * (passDepthFraction(plan, i) - passDepthFraction(plan, i - 1)) * plan.depthDu * plan.flankTan;
        }
        return du;
    }

    double passLengthDu(const PassPlan &plan, int32_t level)
    {
        double length = plan.lengthDu - plan.endReductionDu * (depthLevel(plan, level) - 1);
        return length < 0 ? 0 : length;
    }

    int32_t passNextLevel(const PassPlan &plan, const PassCursor &cursor)
    {
        if (cursor.level == 0) {
            return 1;
        }
        return cursor.startIndex + 1 < plan.starts ? cursor.level : cursor.level + 1 + cursor.skipLevels;
    }

    bool passAdvance(const PassPlan &plan, PassCursor &cursor)
    {
        if (cursor.level == 0) {
            cursor.level = 1;
            return true;
        }
        if (++cursor.startIndex < plan.starts) {
            return true;
        }
        cursor.startIndex = 0;
        cursor.level += 1 + cursor.skipLevels;
        cursor.skipLevels = 0;
        return cursor.level <= passTotalLevels(plan);
    }

    void passSkipLevel(const PassPlan &plan, PassCursor &cursor)
    {
        if (cursor.level > 0 && cursor.level + cursor.skipLevels + 1 < passTotalLevels(plan)) {
            cursor.skipLevels++;
        }
    }

    int32_t passCount(const PassPlan &plan)
    {
        return passTotalLevels(plan) * plan.starts;
    }

    int32_t passNumber(const PassPlan &plan, const PassCursor &cursor)
    {
        if (cursor.level == 0) {
            return 0;
        }
        int32_t pass = (cursor.level - 1) * plan.starts + cursor.startIndex + 1;
        int32_t passes = passCount(plan);
        return pass > passes ? passes : pass;
    }
}
