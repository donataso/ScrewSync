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
#include "ModeParamCodec.hpp"

#include <string.h>

namespace
{
    using Mode::Detail::Ops;
    using Mode::Detail::Param;

    constexpr size_t MAX_PARAMS_SIZE = 128;
}

namespace Mode::Detail
{
    const Param *firstMissing(const Ops &ops)
    {
        return firstUnsetParam(ops.paramTable, ops.paramCount, (const uint8_t *)ops.params);
    }
}

namespace Mode
{
    const char *setParam(Id id, const char *param, const char *value)
    {
        Detail::Guard guard;
        const Ops *ops = Detail::ops(id);
        if (!ops) {
            return "mode not implemented";
        }
        const Param *p = Detail::findParam(ops->paramTable, ops->paramCount, param);
        if (!p) {
            return "unknown param";
        }
        bool live = Detail::isRunning(id);
        if (live && !p->live) {
            return "stop first";
        }
        uint8_t previous[MAX_PARAMS_SIZE];
        memcpy(previous, ops->params, ops->paramsSize);
        const char *refusal = Detail::parseParam(*p, value, (uint8_t *)ops->params);
        if (!refusal && live && !Detail::paramIsSet(*p, (const uint8_t *)ops->params)) {
            refusal = "can't be 0 while running";
        }
        if (!refusal && ops->onChange) {
            ops->onChange(*p);
        }
        if (!refusal && live && ops->onLiveChange) {
            refusal = ops->onLiveChange(previous);
        }
        if (refusal) {
            memcpy(ops->params, previous, ops->paramsSize);
            return refusal;
        }
        Detail::saveParams(id);
        return nullptr;
    }

    const char *loadParams(Id id, const char *line, bool commit)
    {
        Detail::Guard guard;
        const Ops *ops = Detail::ops(id);
        if (!ops) {
            return "mode not implemented";
        }
        if (Detail::isRunning(id)) {
            return "stop first";
        }
        uint8_t params[MAX_PARAMS_SIZE];
        memcpy(params, ops->params, ops->paramsSize);
        const char *refusal = Detail::loadParamLine(ops->paramTable, ops->paramCount, line, params);
        if (refusal) {
            return refusal;
        }
        if (commit) {
            memcpy(ops->params, params, ops->paramsSize);
            Detail::saveParams(id);
        }
        return nullptr;
    }

    bool required(Id id, const char *param)
    {
        const Ops *ops = Detail::ops(id);
        const Param *p = ops ? Detail::findParam(ops->paramTable, ops->paramCount, param) : nullptr;
        return p && p->required;
    }

    bool ready()
    {
        Detail::Guard guard;
        const Ops *ops = Detail::ops(current());
        return ops && !Detail::firstMissing(*ops);
    }

    bool describe(Id id, char *out, size_t outMax)
    {
        Detail::Guard guard;
        const Ops *ops = Detail::ops(id);
        if (!ops) {
            return false;
        }
        Detail::describeParams(ops->paramTable, ops->paramCount, (const uint8_t *)ops->params, out, outMax);
        return true;
    }
}
