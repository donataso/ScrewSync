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
#ifndef WsCommand_h
#define WsCommand_h

#include <zephyr/net/http/server.h>

extern struct http_resource_detail_websocket g_wsCommandDetail;

namespace WsCommand
{
    void begin();

    // "ok" then one numbered line per client slot.
    void describeClients(char *out, size_t outMax);

    // Closes client number (1-based, as listed). nullptr on success, else the reason.
    const char *kill(int number);

    // Nothing should still be listening mid-flash.
    void closeAll();
}

#endif
