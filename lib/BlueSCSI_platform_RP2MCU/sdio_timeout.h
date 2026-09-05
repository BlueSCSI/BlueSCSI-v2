/**
 * BlueSCSI - Copyright (c) 2026 Eric Helgeson <eric@bluescsi.com>
 *
 * BlueSCSI firmware is licensed under the GPL version 3 or any later version.
 *
 * https://www.gnu.org/licenses/gpl-3.0.html
 * ----
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

// Deadline check for a start time that an interrupt rewrites while the main
// loop polls. The start time is read before the clock, so an IRQ landing
// between the two reads can only make the block look younger. Reading the
// clock first lets a start time from the future wrap the subtraction to
// ~4.29e9 ms, which killed SD write bursts that were microseconds old.

#pragma once

#include <stdint.h>
#include <stdbool.h>

static inline bool sdio_deadline_passed(const volatile uint32_t *start_ms, uint32_t (*now_ms)(void), uint32_t limit_ms)
{
    uint32_t start = *start_ms;
    uint32_t now = now_ms();
    return (uint32_t)(now - start) > limit_ms;
}
