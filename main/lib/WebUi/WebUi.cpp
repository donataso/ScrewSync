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
#include "WebUi.hpp"
#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include "Ota.hpp"
#include "StaticAssets.hpp"
#include "WebRoutes.hpp"
#include "WifiPortal.hpp"
#include "WsCommand.hpp"

namespace
{
    uint16_t g_port = 80;
}

HTTP_SERVICE_DEFINE(WEB_UI_HTTP_SERVICE, NULL, &g_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 10, NULL, NULL, NULL);
HTTP_RESOURCE_DEFINE(INDEX_RESOURCE, WEB_UI_HTTP_SERVICE, "/", &g_indexDetail);
HTTP_RESOURCE_DEFINE(WIFI_RESOURCE, WEB_UI_HTTP_SERVICE, "/wifi", &g_wifiPortalDetail);
HTTP_RESOURCE_DEFINE(WS_RESOURCE, WEB_UI_HTTP_SERVICE, "/ws", &g_wsCommandDetail);
HTTP_RESOURCE_DEFINE(UPDATE_RESOURCE, WEB_UI_HTTP_SERVICE, "/update", &WebRoutes::g_update);
#ifdef CONFIG_SCREWSYNC_DISPLAY_IMAGE
HTTP_RESOURCE_DEFINE(UPDATE_DISPLAY_RESOURCE, WEB_UI_HTTP_SERVICE, "/update-display", &WebRoutes::g_updateDisplay);
#endif
HTTP_RESOURCE_DEFINE(PREVIEW_RESOURCE, WEB_UI_HTTP_SERVICE, "/display-preview", &WebRoutes::g_preview);
HTTP_RESOURCE_DEFINE(THREADS_RESOURCE, WEB_UI_HTTP_SERVICE, "/threads", &WebRoutes::g_threads);
HTTP_RESOURCE_DEFINE(PINS_RESOURCE, WEB_UI_HTTP_SERVICE, "/pins", &WebRoutes::g_pins);
HTTP_RESOURCE_DEFINE(BACKUP_RESOURCE, WEB_UI_HTTP_SERVICE, "/backup", &WebRoutes::g_backup);
HTTP_RESOURCE_DEFINE(LOG_RESOURCE, WEB_UI_HTTP_SERVICE, "/log", &WebRoutes::g_logRing);
HTTP_RESOURCE_DEFINE(GCODE_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode", &WebRoutes::g_gcodeList);
HTTP_RESOURCE_DEFINE(GCODE_SAVE_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/save", &WebRoutes::g_gcodeSave);
HTTP_RESOURCE_DEFINE(GCODE_UPDATE_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/update", &WebRoutes::g_gcodeUpdate);
HTTP_RESOURCE_DEFINE(GCODE_RUN_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/run", &WebRoutes::g_gcodeRun);
HTTP_RESOURCE_DEFINE(GCODE_ACTIVE_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/active", &WebRoutes::g_gcodeActive);
HTTP_RESOURCE_DEFINE(GCODE_PROGRAM_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/program", &WebRoutes::g_gcodeProgram);
HTTP_RESOURCE_DEFINE(GCODE_SELECT_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/select", &WebRoutes::g_gcodeSelect);
HTTP_RESOURCE_DEFINE(GCODE_DELETE_RESOURCE, WEB_UI_HTTP_SERVICE, "/gcode/delete", &WebRoutes::g_gcodeDelete);

namespace WebUi
{
    void begin()
    {
        Ota::begin();
        WsCommand::begin();
        http_server_start();
    }

    void poll()
    {
        Ota::selfTestTick((uint32_t)k_uptime_get());
    }
}
