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
#include "Ota.hpp"
#include "Machine.hpp"
#include <string.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(ota, LOG_LEVEL_INF);

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

#define OTA_SELFTEST_MS 10000

namespace
{
    struct flash_img_context g_ctx;
    bool g_active = false;
    bool g_confirmed = true; // becomes false in begin() if this boot is still in test mode
    uint32_t g_bootMs = 0;
}

namespace Ota
{
    void begin()
    {
        g_confirmed = boot_is_img_confirmed();
        if (!g_confirmed) {
            LOG_INF("ota: booted into new image %s, confirming after %u ms uptime", FW_VERSION, OTA_SELFTEST_MS);
        } else {
            LOG_INF("ota: booted into already-confirmed image %s (no swap pending)", FW_VERSION);
        }
    }

    bool writeChunk(const uint8_t *data, size_t len)
    {
        if (!g_active)
        {
            // Entering Flashing while the machine is Running trips EStop
            // instead (see Machine::setFlashing) -- this is the per-chunk
            // recheck's backstop in case spinning/sync starts in the same
            // tick the caller's own safety check already passed.
            if (!Machine::setFlashing(true)) {
                return false;
            }
            // Erases the whole secondary slot up front, in one shot, instead of
            // CONFIG_STREAM_FLASH_ERASE's per-4KB-sector erase as data streams in --
            // same end result (a clean slot to write into), much less time spent
            // erasing overall on this chip.
            if (boot_erase_img_bank(flash_img_get_upload_slot()) != 0) {
                Machine::setFlashing(false);
                return false;
            }
            if (flash_img_init(&g_ctx) != 0) {
                Machine::setFlashing(false);
                return false;
            }
            g_active = true;
        }
        return flash_img_buffered_write(&g_ctx, data, len, false) == 0;
    }

    bool finish()
    {
        if (!g_active) {
            return false;
        }
        bool ok = flash_img_buffered_write(&g_ctx, nullptr, 0, true) == 0;
        g_active = false;
        Machine::setFlashing(false);
        if (!ok) {
            return false;
        }

        const struct flash_area *fa;
        if (flash_area_open(flash_img_get_upload_slot(), &fa) == 0)
        {
            uint8_t hdr[32];
            int rc = flash_area_read(fa, 4096, hdr, sizeof(hdr));
            flash_area_close(fa);
            if (rc == 0) {
                uint32_t magic, loadAddr, imgSize, flags;
                uint16_t hdrSize, tlvSize;
                memcpy(&magic, &hdr[0], 4);
                memcpy(&loadAddr, &hdr[4], 4);
                memcpy(&hdrSize, &hdr[8], 2);
                memcpy(&tlvSize, &hdr[10], 2);
                memcpy(&imgSize, &hdr[12], 4);
                memcpy(&flags, &hdr[16], 4);
                LOG_INF("ota: header magic=0x%08x load_addr=0x%08x hdr_size=%u tlv_size=%u img_size=%u flags=0x%08x",
                        magic, loadAddr, hdrSize, tlvSize, imgSize, flags);
            } else {
                LOG_WRN("ota: readback failed: %d", rc);
            }
        }

        return boot_request_upgrade(0) == 0; // test mode -- confirmed only after selfTestTick()
    }

    void abort()
    {
        g_active = false;
        Machine::setFlashing(false);
    }

    bool isActive()
    {
        return g_active;
    }

    void selfTestTick(uint32_t nowMs)
    {
        if (g_confirmed) {
            return;
        }
        if (g_bootMs == 0) {
            g_bootMs = nowMs ? nowMs : 1;
        }
        if (nowMs - g_bootMs < OTA_SELFTEST_MS) {
            return;
        }
        if (boot_write_img_confirmed() == 0) {
            g_confirmed = true;
            LOG_INF("ota: new image confirmed");
        }
    }
}
