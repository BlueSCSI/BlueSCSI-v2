/**
 * ZuluSCSI™ - Copyright (c) 2025 Rabbit Hole Computing™
 *
 * ZuluSCSI™ firmware is licensed under the GPL version 3 or any later version.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/
#ifndef BLUESCSI_BLINK_H
#define BLUESCSI_BLINK_H

#include <stdint.h>

#define BLINK_STATUS_OK 1
#define BLINK_ERROR_NO_IMAGES  3
#define BLINK_DIRECT_MODE      4
#define BLINK_ERROR_NO_SD_CARD 5

bool blink_poll();
void blink_cancel();
void blinkStatus(uint32_t times, uint32_t delay = 500, uint32_t end_delay = 1250);

#endif /* BLUESCSI_BLINK_H */