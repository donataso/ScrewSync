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
#include "Machine.hpp"
#include "Mode.hpp"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(display_image);

namespace
{
    // Header in the first sector, image from the second.
    constexpr uint32_t MAGIC = 0x31544644; // "DFT1"
    constexpr size_t SECTOR = 4096;
    constexpr off_t DATA_OFFSET = SECTOR;

    struct Header
    {
        uint32_t magic;
        uint32_t length;
        uint32_t crc; // crc32_ieee of the image
        uint32_t reserved;
    };

    // Internal RAM: flash writes stall the cache, so a PSRAM source can't be written directly.
    uint8_t g_chunk[SECTOR] __aligned(4);

    const struct flash_area *openPartition()
    {
        const struct flash_area *fa = nullptr;
        if (flash_area_open(PARTITION_ID(display_partition), &fa) != 0) {
            LOG_ERR("no display partition");
            return nullptr;
        }
        return fa;
    }

    bool readHeader(const struct flash_area *fa, Header &h)
    {
        return flash_area_read(fa, 0, &h, sizeof(h)) == 0 && h.magic == MAGIC && h.length > 0 &&
               DATA_OFFSET + h.length <= fa->fa_size;
    }

    bool storedCrc(const struct flash_area *fa, uint32_t length, uint32_t &crc)
    {
        crc = 0;
        for (uint32_t off = 0; off < length; off += SECTOR) {
            size_t n = MIN(SECTOR, length - off);
            if (flash_area_read(fa, DATA_OFFSET + (off_t)off, g_chunk, n) != 0) {
                return false;
            }
            crc = crc32_ieee_update(crc, g_chunk, n);
        }
        return true;
    }

    // Header sector erased first and written last, so an interrupted store reads as empty.
    const char *writeImage(const uint8_t *image, size_t len)
    {
        const struct flash_area *fa = openPartition();
        if (!fa) {
            return "ok, not stored: no partition";
        }
        uint32_t crc = crc32_ieee(image, len);
        Header h;
        if (readHeader(fa, h) && h.length == len && h.crc == crc) {
            flash_area_close(fa);
            return "ok, already stored";
        }
        if (DATA_OFFSET + len > fa->fa_size) {
            flash_area_close(fa);
            return "ok, not stored: too large";
        }
        bool ok = flash_area_erase(fa, 0, SECTOR) == 0;
        for (size_t off = 0; ok && off < len; off += SECTOR) {
            size_t n = MIN(SECTOR, len - off);
            memcpy(g_chunk, image + off, n);
            size_t padded = ROUND_UP(n, 4);
            memset(g_chunk + n, 0xFF, padded - n);
            ok = flash_area_erase(fa, DATA_OFFSET + (off_t)off, SECTOR) == 0 &&
                 flash_area_write(fa, DATA_OFFSET + (off_t)off, g_chunk, padded) == 0;
        }
        if (ok) {
            h = {MAGIC, (uint32_t)len, crc, 0xFFFFFFFF};
            memcpy(g_chunk, &h, sizeof(h));
            ok = flash_area_write(fa, 0, g_chunk, sizeof(h)) == 0;
        }
        flash_area_close(fa);
        LOG_INF("%s %u bytes", ok ? "stored" : "store failed at", (unsigned)len);
        return ok ? "ok, stored" : "ok, not stored: flash write failed";
    }

    const struct flash_area *g_storeFa = nullptr;
    uint32_t g_storeLen = 0;
    uint32_t g_storeCrc = 0;
    uint32_t g_storeOff = 0;
    size_t g_storeChunkLen = 0;

    bool writeStoreSector()
    {
        size_t padded = ROUND_UP(g_storeChunkLen, 4);
        memset(g_chunk + g_storeChunkLen, 0xFF, padded - g_storeChunkLen);
        bool ok = flash_area_erase(g_storeFa, DATA_OFFSET + (off_t)g_storeOff, SECTOR) == 0 &&
                  flash_area_write(g_storeFa, DATA_OFFSET + (off_t)g_storeOff, g_chunk, padded) == 0;
        g_storeOff += g_storeChunkLen;
        g_storeChunkLen = 0;
        return ok;
    }
}

namespace DisplayImage::Detail
{
    const char *storeImage(const uint8_t *image, size_t len)
    {
        return writeImage(image, len);
    }

    bool readStored(bool (*onLength)(uint32_t length), bool (*onChunk)(const uint8_t *data, size_t len))
    {
        const struct flash_area *fa = openPartition();
        if (!fa) {
            return false;
        }
        Header h;
        uint32_t crc;
        if (!readHeader(fa, h) || !storedCrc(fa, h.length, crc) || crc != h.crc) {
            flash_area_close(fa);
            LOG_ERR("stored image missing or corrupt");
            return false;
        }
        bool ok = onLength(h.length);
        for (uint32_t off = 0; ok && off < h.length; off += SECTOR) {
            size_t n = MIN(SECTOR, h.length - off);
            ok = flash_area_read(fa, DATA_OFFSET + (off_t)off, g_chunk, n) == 0 && onChunk(g_chunk, n);
        }
        flash_area_close(fa);
        return ok;
    }
}

namespace DisplayImage
{
    Info info()
    {
        Info i = {};
        const struct flash_area *fa = openPartition();
        if (!fa) {
            return i;
        }
        Header h;
        if (readHeader(fa, h)) {
            i = {true, h.length, h.crc};
        }
        flash_area_close(fa);
        return i;
    }

    const char *storeBegin(uint32_t len, uint32_t crc)
    {
        if (Machine::state() != Machine::State::Idle || Mode::running()) {
            return "machine not idle";
        }
        if (!claim()) {
            return "display flash in progress";
        }
        g_storeFa = openPartition();
        if (!g_storeFa) {
            release();
            return "no partition";
        }
        if (len == 0 || DATA_OFFSET + len > g_storeFa->fa_size) {
            storeAbort();
            return "bad length";
        }
        // Header erased first and written last, so an interrupted store reads as empty.
        if (flash_area_erase(g_storeFa, 0, SECTOR) != 0) {
            storeAbort();
            return "flash erase failed";
        }
        g_storeLen = len;
        g_storeCrc = crc;
        g_storeOff = 0;
        g_storeChunkLen = 0;
        return nullptr;
    }

    bool storeChunk(const uint8_t *data, size_t len)
    {
        while (len) {
            size_t n = MIN(len, SECTOR - g_storeChunkLen);
            memcpy(g_chunk + g_storeChunkLen, data, n);
            g_storeChunkLen += n;
            data += n;
            len -= n;
            if (g_storeChunkLen == SECTOR && !writeStoreSector()) {
                return false;
            }
        }
        return true;
    }

    const char *storeFinish()
    {
        const char *reason = nullptr;
        uint32_t crc;
        if (g_storeChunkLen && !writeStoreSector()) {
            reason = "flash write failed";
        } else if (g_storeOff != g_storeLen) {
            reason = "length mismatch";
        } else if (!storedCrc(g_storeFa, g_storeLen, crc) || crc != g_storeCrc) {
            reason = "crc mismatch";
        } else {
            Header h = {MAGIC, g_storeLen, g_storeCrc, 0xFFFFFFFF};
            memcpy(g_chunk, &h, sizeof(h));
            if (flash_area_write(g_storeFa, 0, g_chunk, sizeof(h)) != 0) {
                reason = "header write failed";
            }
        }
        storeAbort();
        if (!reason) {
            LOG_INF("stored %u bytes", (unsigned)g_storeLen);
        }
        return reason;
    }

    void storeAbort()
    {
        if (g_storeFa) {
            flash_area_close(g_storeFa);
            g_storeFa = nullptr;
        }
        g_storeChunkLen = 0;
        release();
    }
}
