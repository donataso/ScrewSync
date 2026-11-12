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
#include "WifiInternal.hpp"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/fcntl.h>

LOG_MODULE_DECLARE(main);

using namespace Wifi::Detail;

namespace
{
    const int64_t WATCHDOG_INTERVAL_MS = 60000;
    const int WATCHDOG_PROBE_TIMEOUT_MS = 3000;
    const int64_t WATCHDOG_FAST_FAIL_MS = 800;
    const uint8_t WATCHDOG_MAX_MISSES = 3;
    int64_t g_wdAt = 0;
    uint8_t g_wdMisses = 0;

    // True if the gateway answered on port 80, or refused fast enough to be alive.
    bool probeGateway(net_in_addr gw)
    {
        int fd = zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
        if (fd < 0) {
            return true;
        }
        zsock_fcntl(fd, F_SETFL, O_NONBLOCK);

        net_sockaddr_in addr = {};
        addr.sin_family = NET_AF_INET;
        addr.sin_port = net_htons(80);
        addr.sin_addr = gw;

        int64_t t0 = k_uptime_get();
        int rc = zsock_connect(fd, (net_sockaddr *)&addr, sizeof(addr));
        bool ok = (rc == 0);
        if (rc < 0 && errno == EINPROGRESS) {
            zsock_pollfd pfd = {fd, ZSOCK_POLLOUT, 0};
            if (zsock_poll(&pfd, 1, WATCHDOG_PROBE_TIMEOUT_MS) > 0) {
                int err = 0;
                net_socklen_t errLen = sizeof(err);
                zsock_getsockopt(fd, ZSOCK_SOL_SOCKET, ZSOCK_SO_ERROR, &err, &errLen);
                ok = (err == 0);
            }
        }
        int64_t elapsed = k_uptime_get() - t0;
        zsock_close(fd);
        return ok || elapsed < WATCHDOG_FAST_FAIL_MS;
    }
}

namespace Wifi::Detail
{
    void armWatchdog()
    {
        g_wdMisses = 0;
        g_wdAt = k_uptime_get() + WATCHDOG_INTERVAL_MS;
    }

    // A closed port answers fast, a stale route times out.
    void serviceWatchdog()
    {
        if (k_uptime_get() < g_wdAt) {
            return;
        }
        g_wdAt = k_uptime_get() + WATCHDOG_INTERVAL_MS;

        net_in_addr gw = net_if_ipv4_get_gw(g_iface);
        if (gw.s_addr == 0) {
            return;
        }
        if (probeGateway(gw)) {
            g_wdMisses = 0;
            return;
        }
        g_wdMisses++;
        LOG_WRN("wifi: gateway unreachable (miss %u/%u)", (unsigned)g_wdMisses, (unsigned)WATCHDOG_MAX_MISSES);
        if (g_wdMisses >= WATCHDOG_MAX_MISSES) {
            g_wdMisses = 0;
            LOG_INF("wifi: watchdog forcing reconnect (stale route?)");
            beginConnectSequence();
        }
    }
}
