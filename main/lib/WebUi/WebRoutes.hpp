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
#ifndef WebRoutes_h
#define WebRoutes_h

#include <zephyr/net/http/server.h>

namespace WebRoutes
{
    extern struct http_resource_detail_dynamic update;
    extern struct http_resource_detail_dynamic updateDisplay;
    extern struct http_resource_detail_dynamic preview;
    extern struct http_resource_detail_dynamic threads;
    extern struct http_resource_detail_dynamic backup;
    extern struct http_resource_detail_dynamic logRing;
    extern struct http_resource_detail_dynamic gcodeList;
    extern struct http_resource_detail_dynamic gcodeSave;
    extern struct http_resource_detail_dynamic gcodeUpdate;
    extern struct http_resource_detail_dynamic gcodeRun;
    extern struct http_resource_detail_dynamic gcodeActive;
    extern struct http_resource_detail_dynamic gcodeProgram;
    extern struct http_resource_detail_dynamic gcodeSelect;
    extern struct http_resource_detail_dynamic gcodeDelete;
}

#endif
