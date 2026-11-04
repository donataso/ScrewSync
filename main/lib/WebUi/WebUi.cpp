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

HTTP_SERVICE_DEFINE(webUiHttpService, NULL, &g_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 10, NULL, NULL, NULL);
HTTP_RESOURCE_DEFINE(g_indexResource, webUiHttpService, "/", &indexDetail);
HTTP_RESOURCE_DEFINE(g_wifiResource, webUiHttpService, "/wifi", &wifiPortalDetail);
HTTP_RESOURCE_DEFINE(g_wsResource, webUiHttpService, "/ws", &wsCommandDetail);
HTTP_RESOURCE_DEFINE(g_updateResource, webUiHttpService, "/update", &WebRoutes::update);
HTTP_RESOURCE_DEFINE(g_updateDisplayResource, webUiHttpService, "/update-display", &WebRoutes::updateDisplay);
HTTP_RESOURCE_DEFINE(g_previewResource, webUiHttpService, "/display-preview", &WebRoutes::preview);
HTTP_RESOURCE_DEFINE(g_threadsResource, webUiHttpService, "/threads", &WebRoutes::threads);
HTTP_RESOURCE_DEFINE(g_backupResource, webUiHttpService, "/backup", &WebRoutes::backup);
HTTP_RESOURCE_DEFINE(g_logResource, webUiHttpService, "/log", &WebRoutes::logRing);
HTTP_RESOURCE_DEFINE(g_gcodeResource, webUiHttpService, "/gcode", &WebRoutes::gcodeList);
HTTP_RESOURCE_DEFINE(g_gcodeSaveResource, webUiHttpService, "/gcode/save", &WebRoutes::gcodeSave);
HTTP_RESOURCE_DEFINE(g_gcodeUpdateResource, webUiHttpService, "/gcode/update", &WebRoutes::gcodeUpdate);
HTTP_RESOURCE_DEFINE(g_gcodeRunResource, webUiHttpService, "/gcode/run", &WebRoutes::gcodeRun);
HTTP_RESOURCE_DEFINE(g_gcodeActiveResource, webUiHttpService, "/gcode/active", &WebRoutes::gcodeActive);
HTTP_RESOURCE_DEFINE(g_gcodeProgramResource, webUiHttpService, "/gcode/program", &WebRoutes::gcodeProgram);
HTTP_RESOURCE_DEFINE(g_gcodeSelectResource, webUiHttpService, "/gcode/select", &WebRoutes::gcodeSelect);
HTTP_RESOURCE_DEFINE(g_gcodeDeleteResource, webUiHttpService, "/gcode/delete", &WebRoutes::gcodeDelete);

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
