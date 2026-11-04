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
#include "DisplayImage.hpp"
#include "DisplayImageStore.hpp"
#include "Display.hpp"
#include "Machine.hpp"
#include "Mode.hpp"
#include "Nextion.hpp"
#include "config.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(display_image, LOG_LEVEL_INF);

namespace
{
    atomic_t g_busy = ATOMIC_INIT(0);
    K_SEM_DEFINE(g_start, 0, 1);

    bool waitReconnect()
    {
        int64_t end = k_uptime_get() + DISPLAY_RECONNECT_TIMEOUT_MS;
        while (!Display::connected())
        {
            if (k_uptime_get() >= end) {
                return false;
            }
            k_sleep(K_MSEC(100));
        }
        return true;
    }

    bool flashFromPartition()
    {
        bool ok = DisplayImage::detail::readStored(Nextion::flashBegin, Nextion::flashChunk) && Nextion::flashFinish();
        if (!ok) {
            Nextion::flashAbort();
        }
        return ok;
    }

    void worker(void *, void *, void *)
    {
        while (true)
        {
            k_sem_take(&g_start, K_FOREVER);
            LOG_INF("flashing stored image");
            bool ok = flashFromPartition();
            DisplayImage::endFlash();
            LOG_INF("stored image %s", ok ? "flashed" : "flash failed");
        }
    }
}

K_THREAD_DEFINE(g_displayImageWorker, 4096, worker, nullptr, nullptr, nullptr, K_PRIO_PREEMPT(7), 0, 0);

namespace DisplayImage
{
    bool claim()
    {
        return atomic_cas(&g_busy, 0, 1);
    }

    void release()
    {
        atomic_set(&g_busy, 0);
    }

    const char *beginFlash()
    {
        if (!claim()) {
            return "display flash in progress";
        }
        if (!Machine::setFlashing(true))
        {
            release();
            return "machine not idle";
        }
        return nullptr;
    }

    void endFlash()
    {
        Machine::setFlashing(false);
        release();
    }

    bool flash(const uint8_t *image, size_t len, bool store, const char *&reply)
    {
        bool ok = Nextion::flashBegin(len) && Nextion::flashChunk(image, len) && Nextion::flashFinish();
        if (!ok)
        {
            Nextion::flashAbort();
            reply = "display flash failed";
            return false;
        }
        if (!store) {
            reply = "ok";
        } else if (!waitReconnect()) {
            reply = "ok, not stored: display did not reconnect";
        } else {
            reply = detail::storeImage(image, len);
        }
        return true;
    }

    const char *flashStored()
    {
        if (Machine::state() != Machine::State::Idle || Mode::running()) {
            return "machine not idle";
        }
        if (!info().stored) {
            return "nothing stored";
        }
        const char *refusal = beginFlash();
        if (!refusal) {
            k_sem_give(&g_start);
        }
        return refusal;
    }
}
