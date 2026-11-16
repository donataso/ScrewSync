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
        int32_t speedStartDuPerMin;
        int32_t accelerationDuPerSec2;
        int32_t decelerationDuPerSec2;
        int32_t speedRapidDuPerMin;
        bool invertDirection;
        bool invertEnable;
        int32_t maxTravelMm;
        int32_t backlashDu;
        float handwheelPpr;
        bool invertHandwheel;
        // How far the handwheel target may lead the axis, as travel time at the rapid speed.
        int32_t handwheelLagMs;
        int32_t handwheelTravelDu;
        int32_t homeFastDuPerMin;
        int32_t homeSlowDuPerMin;
        int32_t homeLatchBackOffDu;
        int32_t homePullOffDu;
        int32_t homePositionDu;

        int32_t speedStartSteps;
        int32_t accelerationSteps;
        int32_t decelerationSteps;
        int32_t speedRapidSteps;
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
        int32_t probeDstDu = 0;               // 0 = not set
        int32_t probeOffsetDu = 0;

        bool requireHoming = false;
        int32_t bootState = 0;

        bool xMotorEnabled = true;
        bool homingEnabled = false;
        bool probeEnabled = false;

        Axis z{20000, 1600, 500000, 100000, 250000, 10000000, true, false, 300, 0, 600, false, 100, 250000, 5000000, 250000, 20000, 20000, 0, 0, 0, 0, 0};
        Axis x{20000, 1600, 500000, 100000, 250000, 5000000, false, false, 100, 0, 600, false, 100, 20000, 2500000, 250000, 20000, 20000, 0, 0, 0, 0, 0};
    };

    extern Conf g_active;

    constexpr size_t CONF_JSON_FIELDS = 58;
    extern const struct json_obj_descr CONF_JSON_DESCR[CONF_JSON_FIELDS];

    struct Wired
    {
        bool homing;
        bool probe;
    };

    void load();
    void applyLoadedFeatureDefaults(const Wired &wired);
    void applyFeatureDefaults(Conf &conf, int64_t mask, const Wired &wired);
    void resetFeatures(Conf &conf, const Wired &wired);
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
