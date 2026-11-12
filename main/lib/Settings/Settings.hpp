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
#ifndef Settings_h
#define Settings_h

#include <stddef.h>
#include <stdint.h>
#include "config.hpp"
#include <zephyr/data/json.h>

namespace Settings
{
    struct Axis
    {
        int32_t screwPitchDu;
        int32_t motorSteps;
        int32_t speedStart;
        int32_t acceleration;
        int32_t deceleration;
        int32_t speedManualMove;
        bool invertDirection;
        bool invertEnable;
        int32_t maxTravelMm;
        int32_t backlashDu;
        float handwheelPpr;
        bool invertHandwheel;
    };

    struct Conf
    {
        int32_t encoderPpr = 1200;
        int32_t encoderBacklash = 3;
        bool invertEncoder = false;
        int32_t axisEncoderBacklash = 0;
        int32_t stepTimeMs = 500;
        int32_t delayBetweenStepsMs = 80;
        int32_t safeDistanceDu = 10000;
        int32_t slotLeftReductionDu = 0;
        int32_t statusPushIntervalMs = DEFAULT_STATUS_PUSH_INTERVAL_MS;

        // Conservative defaults: slow feeds, since a switch probe has only
        // a little travel left after it triggers (docs/probe.md).
        bool probePolarityNc = false;
        int32_t probeFeedDuPerMin = 300000;   // 30 mm/min
        int32_t reprobeFeedDuPerMin = 100000; // 10 mm/min
        int32_t probeBackOffDu = 5000;        // 0.5 mm

        Axis z{20000, 1600, 800, 10000, 20000, 12800, true, false, 300, 0, 600, false};
        Axis x{20000, 1600, 800, 10000, 20000, 6400, false, false, 100, 0, 600, false};
    };

    extern Conf g_active;

    constexpr size_t CONF_JSON_FIELDS = 37;
    extern const struct json_obj_descr CONF_JSON_DESCR[CONF_JSON_FIELDS];

    void load();
    bool save();
    bool setByName(const char *name, const char *value, char *out, size_t outMax);
    void formatAll(char *out, size_t outMax);
    bool setAllFromJson(const char *json, size_t len, char *out, size_t outMax);
    // False with the reason in out if any field is out of range.
    bool validate(const Conf &conf, char *out, size_t outMax);
    // Replaces active with an already validated conf and persists it.
    bool replace(const Conf &conf);
}

#endif
