/** 
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
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

#include "scsiHostPhy.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "scsi_accel_host.h"
#include <assert.h>

#include <scsi2sd.h>
extern "C" {
#include <scsi.h>
}

volatile int g_scsiHostPhyReset;
int g_scsiHostBusWidth;
bool perform_parity_checking = true;

#ifndef PLATFORM_HAS_INITIATOR_MODE

// Dummy functions for platforms without hardware support for
// SCSI initiator mode.
void scsiHostPhyReset(void) {}
bool scsiHostPhySelect(int target_id) { return false; }
int scsiHostPhyGetPhase() { return 0; }
bool scsiHostRequestWaiting() { return false; }
uint32_t scsiHostWrite(const uint8_t *data, uint32_t count) { return 0; }
uint32_t scsiHostRead(uint8_t *data, uint32_t count) { return 0; }
void scsiHostPhyRelease();

#else

// Release bus and pulse RST signal, initialize PHY to host mode.
void scsiHostPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    SCSI_ENABLE_INITIATOR();

    scsi_accel_host_init();
#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    SCSI_OUT(RST, 0);  // Inverted RST output logic
#else
    SCSI_OUT(RST, 1);
#endif
    platform_delay_ms(2);
#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    SCSI_OUT(RST, 1);  // Inverted RST output logic
#else
    SCSI_OUT(RST, 0);
#endif
    platform_delay_ms(250);
    g_scsiHostPhyReset = false;
}

// Select a device and an initiator, ids 0-7.
// Returns true if the target answers to selection request.
bool scsiHostPhySelect(int target_id, uint8_t initiator_id)
{
    // Command phase always happens in 8-bit mode
    scsiHostSetBusWidth(0);

    // We can't write individual data bus bits, so use a bit modified
    // arbitration scheme. We always yield to any other initiator on
    // the bus.
    scsiLogInitiatorPhaseChange(BUS_BUSY);
    SCSI_OUT(REQ, 0);
    SCSI_OUT(BSY, 1);
    for (int wait = 0; wait < 10; wait++)
    {
        platform_delay_us(1);

        if (SCSI_IN_DATA() != 0)
        {
            dbgmsg("scsiHostPhySelect: bus is busy");
            scsiLogInitiatorPhaseChange(BUS_FREE);
            SCSI_RELEASE_OUTPUTS();
            return false;
        }
    }

    // Selection phase
    scsiLogInitiatorPhaseChange(SELECTION);
    dbgmsg("------ SELECTING ", target_id, " with initiator ID ", (int)initiator_id);
    SCSI_OUT(SEL, 1);
    platform_delay_us(5);
    SCSI_OUT_DATA((1 << target_id) | (1 << initiator_id));
    platform_delay_us(5);
    SCSI_OUT(BSY, 0);

    // Wait for target to respond
    for (int wait = 0; wait < 2500; wait++)
    {
        platform_delay_us(100);
        if (SCSI_IN(BSY))
        {
            break;
        }
    }

    if (!SCSI_IN(BSY))
    {
        // No response
        SCSI_RELEASE_OUTPUTS();
        return false;
    }

    SCSI_RELEASE_DATA_REQ();
    SCSI_OUT(SEL, 0);

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    boolean success = platform_enable_initiator_signals();
    if (!success) {
        logmsg("------ ERROR: Failed To Enable Initiator Signals");
        return false;
    }
#endif
    SCSI_ENABLE_INITIATOR();
    return true;
}

// Read the current communication phase as signaled by the target
int scsiHostPhyGetPhase()
{
    static absolute_time_t last_online_time;

    if (g_scsiHostPhyReset)
    {
        // Reset request from watchdog timer
        scsiHostPhyRelease();
        return BUS_FREE;
    }

    int phase = 0;
    bool req_in = SCSI_IN(REQ);
    if (SCSI_IN(CD)) phase |= __scsiphase_cd;
    if (SCSI_IN(IO)) phase |= __scsiphase_io;
    if (SCSI_IN(MSG)) phase |= __scsiphase_msg;

    if (phase == 0 && absolute_time_diff_us(last_online_time, get_absolute_time()) > 100)
    {

        platform_delay_us(1);

        if (!SCSI_IN(BSY))
        {
            scsiLogInitiatorPhaseChange(BUS_FREE);
            return BUS_FREE;
        }

        last_online_time = get_absolute_time();
    }
    else if (phase != 0)
    {
        last_online_time = get_absolute_time();
    }

    if (!req_in)
    {
        // Don't act on phase changes until target asserts request signal.
        // This filters out any spurious changes on control signals.
        return BUS_BUSY;
    }
    else
    {
        scsiLogInitiatorPhaseChange(phase);
        return phase;
    }
}

bool scsiHostRequestWaiting()
{
    return SCSI_IN(REQ);
}

void scsiHostPhySetATN(bool atn)
{
    SCSI_OUT(ATN, atn);
}

void scsiHostSetBusWidth(int busWidth)
{
#ifdef BLUESCSI_ULTRA_WIDE
    g_scsiHostBusWidth = busWidth;
#else
    assert(busWidth == 0);
#endif
}

// Blocking data transfer
#define SCSIHOST_WAIT_ACTIVE(pin) \
  if (!SCSI_IN(pin)) { \
    if (!SCSI_IN(pin)) { \
      while(!SCSI_IN(pin) && !g_scsiHostPhyReset); \
    } \
  }

#define SCSIHOST_WAIT_INACTIVE(pin) \
  if (SCSI_IN(pin)) { \
    if (SCSI_IN(pin)) { \
      while(SCSI_IN(pin) && !g_scsiHostPhyReset); \
    } \
  }

// Write one byte to SCSI target using the handshake mechanism
static inline void scsiHostWriteOneByte(uint8_t value)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    SCSI_OUT_DATA(value);
    delay_100ns(); // DB setup time before ACK
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_RELEASE_DATA_REQ();
    SCSI_OUT(ACK, 0);
}

// Read one byte from SCSI target using the handshake mechanism.
static inline uint8_t scsiHostReadOneByte(int* parityError)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    uint32_t r = SCSI_IN_DATA();
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_OUT(ACK, 0);

    if (parityError && !scsi_check_parity(r))
    {
        logmsg("Parity error in scsiReadOneByte(): ", (uint32_t)r);
        *parityError = 1;
    }

    return (uint8_t)r;
}

#ifdef BLUESCSI_ULTRA_WIDE
static inline void scsiHostWriteOneWord(uint16_t value)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    SCSI_OUT_DATA(value);
    delay_100ns(); // DB setup time before ACK
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_RELEASE_DATA_REQ();
    SCSI_OUT(ACK, 0);
}

// Read one byte from SCSI target using the handshake mechanism.
static inline uint16_t scsiHostReadOneWord(int* parityError)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    uint32_t r = SCSI_IN_DATA();
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_OUT(ACK, 0);

    if (parityError && !scsi_check_parity_16bit(r))
    {
        logmsg("Parity error in scsiHostReadOneWord(): ", (uint32_t)r);
        *parityError = 1;
    }

    return (uint16_t)r;
}
#endif

uint32_t scsiHostWrite(const uint8_t *data, uint32_t count)
{
    scsiLogDataOut(data, count);

    int cd_start = SCSI_IN(CD);
    int msg_start = SCSI_IN(MSG);

    for (uint32_t i = 0; i < count; i++)
    {
        while (!SCSI_IN(REQ))
        {
            if (g_scsiHostPhyReset || SCSI_IN(IO) || SCSI_IN(CD) != cd_start || SCSI_IN(MSG) != msg_start)
            {
                // Target switched out of DATA_OUT mode
                logmsg("scsiHostWrite: sent ", (int)i, " bytes, expected ", (int)count);
                return i;
            }
        }
        if (g_scsiHostBusWidth == 0)
        {
            scsiHostWriteOneByte(data[i]);
        }
#ifdef BLUESCSI_ULTRA_WIDE
        else if (g_scsiHostBusWidth == 1)
        {
            uint16_t word = data[i++];
            if (i < count) word |= (uint16_t)data[i] << 8;
            scsiHostWriteOneWord(word);
        }
#endif
        else
        {
            logmsg("Invalid bus width ", g_scsiHostBusWidth);
            return 0;
        }
    }

    return count;
}

uint32_t scsiHostRead(uint8_t *data, uint32_t count)
{
    int parityError = 0;
    uint32_t fullcount = count;

    int cd_start = SCSI_IN(CD);
    int msg_start = SCSI_IN(MSG);

    if ((count & 1) == 0 && ((uint32_t)data & 1) == 0)
    {
        // Even number of bytes, use accelerated routine
        count = scsi_accel_host_read(data, count, &parityError, g_scsiHostBusWidth, &g_scsiHostPhyReset);
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            uint32_t start = platform_millis();
            while (!SCSI_IN(REQ) && (platform_millis() - start) < 10000)
            {
                // Wait for REQ asserted
            }

            int io = SCSI_IN(IO);
            int cd = SCSI_IN(CD);
            int msg = SCSI_IN(MSG);

            if (g_scsiHostPhyReset)
            {
                dbgmsg("sciHostRead: aborting due to reset request");
                count = i;
                break;
            }
            else if (!io || cd != cd_start || msg != msg_start)
            {
                dbgmsg("scsiHostRead: aborting because target switched transfer phase (IO: ", io, ", CD: ", cd, ", MSG: ", msg, ")");
                count = i;
                break;
            }

            if (g_scsiHostBusWidth == 0)
            {
                data[i] = scsiHostReadOneByte(&parityError);
            }
#ifdef BLUESCSI_ULTRA_WIDE
            else if (g_scsiHostBusWidth == 1)
            {
                uint16_t word = scsiHostReadOneWord(&parityError);
                data[i++] = word & 0xFF;
                if (i < count) data[i] = word >> 8;
            }
#endif
            else
            {
                logmsg("Invalid bus width ", g_scsiHostBusWidth);
                return 0;
            }
        }
    }

    scsiLogDataIn(data, count);

    if (g_scsiHostPhyReset || parityError)
    {
        return 0;
    }
    else
    {
        if (count < fullcount)
        {
            logmsg("scsiHostRead: received ", (int)count, " bytes, expected ", (int)fullcount);
        }

        return count;
    }
}

// Release bus signals and expect the target to do the same.
// Cycles ACK in case target still holds BSY and REQ.
void scsiHostWaitBusFree()
{
    SCSI_RELEASE_OUTPUTS();

    sleep_us(2);

    // Wait for the target to release BSY signal.
    // If the target is expecting more data, transfer dummy bytes.
    // This happens for some reason with READ6 command on IBM H3171-S2.
    uint32_t start = platform_millis();
    int extra_bytes = 0;
    while (SCSI_IN(BSY))
    {
        platform_poll();

        if (SCSI_IN(REQ))
        {
            // Target is expecting something more
            // Transfer dummy bytes
#if  !(defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE))
             SCSI_OUT(BSY, 1);
#endif
             sleep_us(1);

             while (SCSI_IN(REQ))
             {
                scsiHostReadOneByte(nullptr);
                extra_bytes++;
                sleep_us(1);
             }

#if !(defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE))
             SCSI_OUT(BSY, 0);
#endif
             sleep_us(1);
        }

        if ((uint32_t)(platform_millis() - start) > 10000)
        {
            logmsg("Target is holding BSY for unexpectedly long, running reset.");
            scsiHostPhyReset();
            break;
        }
    }

    if (extra_bytes > 0)
    {
        dbgmsg("---- Target requested ", extra_bytes, " extra bytes after command complete");
    }

    scsiHostPhyRelease();
}

// Release all bus signals
void scsiHostPhyRelease()
{
    scsiLogInitiatorPhaseChange(BUS_FREE);
    SCSI_RELEASE_OUTPUTS();

#if defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE)
    bool success = platform_disable_initiator_signals();
    if (!success) {
        logmsg("------ ERROR: Failed To Disable Initiator Signals");
    }
#endif
}

void setInitiatorModeParityCheck(const bool checkParity) {
    perform_parity_checking = checkParity;
}
#endif
