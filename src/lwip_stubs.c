/**
 * Copyright (C) 2026 Eric Helgeson
 *
 * This file is part of BlueSCSI
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

/**
 * Minimal lwIP stubs for CYW43 with CYW43_LWIP=0
 *
 * The CYW43 driver references pbuf_copy_partial even when lwIP is disabled.
 * This stub satisfies the linker without pulling in the full lwIP stack.
 */

#include <stdint.h>
#include <stddef.h>

// Minimal pbuf structure matching what CYW43 expects
struct pbuf {
    struct pbuf *next;
    void *payload;
    uint16_t tot_len;
    uint16_t len;
};

uint16_t pbuf_copy_partial(const struct pbuf *buf, void *dataptr, uint16_t len, uint16_t offset)
{
    if (!buf || !dataptr) return 0;

    uint16_t copied = 0;
    const struct pbuf *p = buf;

    // Skip to the right pbuf for the offset
    while (p && offset >= p->len) {
        offset -= p->len;
        p = p->next;
    }

    // Copy data
    while (p && copied < len) {
        uint16_t avail = p->len - offset;
        uint16_t tocopy = (len - copied < avail) ? (len - copied) : avail;
        __builtin_memcpy((uint8_t *)dataptr + copied, (uint8_t *)p->payload + offset, tocopy);
        copied += tocopy;
        offset = 0;
        p = p->next;
    }

    return copied;
}
