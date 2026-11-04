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
#include "WsMessages.hpp"
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/sys/util.h>
#include "Settings.hpp"
#include "Status.hpp"
#include "Wifi.hpp"

namespace
{
    const struct json_obj_descr AXIS_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(Status::Axis, pos, JSON_TOK_INT64),
        JSON_OBJ_DESCR_PRIM(Status::Axis, moving, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(Status::Axis, enabled, JSON_TOK_TRUE),
    };

    const struct json_obj_descr GEAR_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(Status::Gear, pitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Status::Gear, axis, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(Status::Gear, stopAfterDu, JSON_TOK_NUMBER),
    };

    const struct json_obj_descr JOB_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(Mode::Progress, phase, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(Mode::Progress, pass, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(Mode::Progress, passes, JSON_TOK_NUMBER),
    };

    struct StatusMsg
    {
        const char *type;
        Status::Snapshot snap;
        const char *params;
    };

    const struct json_obj_descr STATUS_MSG_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(StatusMsg, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_OBJECT_NAMED(StatusMsg, "z", snap.z, AXIS_DESCR),
        JSON_OBJ_DESCR_OBJECT_NAMED(StatusMsg, "x", snap.x, AXIS_DESCR),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "rpm", snap.rpm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "angleDeg", snap.angleDeg, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "state", snap.state, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "mode", snap.mode, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "running", snap.running, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "units", snap.units, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "jogStepDu", snap.jogStepDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_OBJECT_NAMED(StatusMsg, "gear", snap.gear, GEAR_DESCR),
        JSON_OBJ_DESCR_OBJECT_NAMED(StatusMsg, "job", snap.job, JOB_DESCR),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "feedOverride", snap.feedOverridePct, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "error", snap.error, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(StatusMsg, params, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM_NAMED(StatusMsg, "probeContact", snap.probeContact, JSON_TOK_TRUE),
    };

    struct SettingsMsg
    {
        const char *type;
        Settings::Conf conf;
    };

    const struct json_obj_descr SETTINGS_MSG_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(SettingsMsg, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "encoderPpr", conf.encoderPpr, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "encoderBacklash", conf.encoderBacklash, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertEncoder", conf.invertEncoder, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "axisEncoderBacklash", conf.axisEncoderBacklash, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "stepTimeMs", conf.stepTimeMs, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "delayBetweenStepsMs", conf.delayBetweenStepsMs, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "safeDistanceDu", conf.safeDistanceDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "slotLeftReductionDu", conf.slotLeftReductionDu, JSON_TOK_NUMBER),

        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "probePolarityNc", conf.probePolarityNc, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "probeFeedDuPerMin", conf.probeFeedDuPerMin, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "reprobeFeedDuPerMin", conf.reprobeFeedDuPerMin, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "probeBackOffDu", conf.probeBackOffDu, JSON_TOK_NUMBER),

        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "screwPitchDuZ", conf.z.screwPitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "motorStepsZ", conf.z.motorSteps, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "speedStartZ", conf.z.speedStart, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "accelerationZ", conf.z.acceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "speedManualMoveZ", conf.z.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertDirectionZ", conf.z.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertEnableZ", conf.z.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "maxTravelMmZ", conf.z.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "backlashDuZ", conf.z.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "handwheelPprZ", conf.z.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertHandwheelZ", conf.z.invertHandwheel, JSON_TOK_TRUE),

        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "screwPitchDuX", conf.x.screwPitchDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "motorStepsX", conf.x.motorSteps, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "speedStartX", conf.x.speedStart, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "accelerationX", conf.x.acceleration, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "speedManualMoveX", conf.x.speedManualMove, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertDirectionX", conf.x.invertDirection, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertEnableX", conf.x.invertEnable, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "maxTravelMmX", conf.x.maxTravelMm, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "backlashDuX", conf.x.backlashDu, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "handwheelPprX", conf.x.handwheelPpr, JSON_TOK_FLOAT_FP),
        JSON_OBJ_DESCR_PRIM_NAMED(SettingsMsg, "invertHandwheelX", conf.x.invertHandwheel, JSON_TOK_TRUE),
    };

    struct SavedNet
    {
        const char *ssid;
        bool pref;
    };

    const struct json_obj_descr SAVED_NET_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(SavedNet, ssid, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(SavedNet, pref, JSON_TOK_TRUE),
    };

    struct WifiMsg
    {
        const char *type;
        const char *mode;
        const char *ip;
        int32_t rssi;
        const char *ssid;
        const char *apName;
        bool enabled;
        bool fallback;
        int32_t savedCount;
        SavedNet saved[WIFI_MAX_APS];
        size_t savedLen;
    };

    const struct json_obj_descr WIFI_MSG_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(WifiMsg, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(WifiMsg, mode, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(WifiMsg, ip, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(WifiMsg, rssi, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(WifiMsg, ssid, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(WifiMsg, apName, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(WifiMsg, enabled, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(WifiMsg, fallback, JSON_TOK_TRUE),
        JSON_OBJ_DESCR_PRIM(WifiMsg, savedCount, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_OBJ_ARRAY(WifiMsg, saved, WIFI_MAX_APS, savedLen, SAVED_NET_DESCR, ARRAY_SIZE(SAVED_NET_DESCR)),
    };

    struct ScanAp
    {
        const char *ssid;
        int32_t rssi;
        bool locked;
    };

    const struct json_obj_descr SCAN_AP_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ScanAp, ssid, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(ScanAp, rssi, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM(ScanAp, locked, JSON_TOK_TRUE),
    };

    struct ScanMsg
    {
        const char *type;
        ScanAp aps[WIFI_SCAN_KEEP];
        size_t apsLen;
    };

    const struct json_obj_descr SCAN_MSG_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(ScanMsg, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_OBJ_ARRAY(ScanMsg, aps, WIFI_SCAN_KEEP, apsLen, SCAN_AP_DESCR, ARRAY_SIZE(SCAN_AP_DESCR)),
    };

    struct TypedMsg
    {
        const char *type;
        const char *msg;
    };

    const struct json_obj_descr TYPED_MSG_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(TypedMsg, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(TypedMsg, msg, JSON_TOK_STRING),
    };

    struct CmdRequest
    {
        const char *type;
        const char *line;
    };

    const struct json_obj_descr CMD_REQUEST_DESCR[] = {
        JSON_OBJ_DESCR_PRIM(CmdRequest, type, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(CmdRequest, line, JSON_TOK_STRING),
    };
    constexpr int CMD_REQUEST_LINE_BIT = 1;

    template <size_t N, typename T>
    int encode(const struct json_obj_descr (&descr)[N], const T &msg, char *out, size_t outMax)
    {
        return json_obj_encode_buf(descr, N, &msg, out, outMax) == 0 ? (int)strlen(out) : -1;
    }
}

namespace WsMessages
{
    int status(char *out, size_t outMax)
    {
        StatusMsg m{"status", Status::get(), nullptr};
        m.params = m.snap.params;
        return encode(STATUS_MSG_DESCR, m, out, outMax);
    }

    int settings(char *out, size_t outMax)
    {
        SettingsMsg m{"settings", Settings::active};
        return encode(SETTINGS_MSG_DESCR, m, out, outMax);
    }

    int wifi(char *out, size_t outMax)
    {
        WifiMsg m{"wifi",          Wifi::modeName(),   Wifi::ip(),
                  Wifi::rssi(),    Wifi::ssid(),       Wifi::apName(),
                  Wifi::enabled(), Wifi::apFallback(), (int32_t)Wifi::savedCount(),
                  {},              0};
        uint8_t n = MIN(Wifi::savedCount(), ARRAY_SIZE(m.saved));
        for (uint8_t i = 0; i < n; i++) {
            m.saved[i] = {Wifi::savedSsid(i), Wifi::savedIsPreferred(i)};
        }
        m.savedLen = n;
        return encode(WIFI_MSG_DESCR, m, out, outMax);
    }

    int scan(char *out, size_t outMax)
    {
        ScanMsg m{"scan", {}, 0};
        uint8_t n = MIN(Wifi::scanCount(), ARRAY_SIZE(m.aps));
        for (uint8_t i = 0; i < n; i++) {
            m.aps[i] = {Wifi::scanSsid(i), Wifi::scanRssi(i), Wifi::scanLocked(i)};
        }
        m.apsLen = n;
        return encode(SCAN_MSG_DESCR, m, out, outMax);
    }

    int typed(char *out, size_t outMax, const char *type, const char *msg)
    {
        TypedMsg m{type, msg};
        return encode(TYPED_MSG_DESCR, m, out, outMax);
    }

    bool parseCommand(char *json, size_t len, const char *&line)
    {
        CmdRequest req = {};
        int64_t mask = json_obj_parse(json, len, CMD_REQUEST_DESCR, ARRAY_SIZE(CMD_REQUEST_DESCR), &req);
        if (mask < 0 || !(mask & BIT(CMD_REQUEST_LINE_BIT))) {
            return false;
        }
        line = req.line;
        return true;
    }
}
