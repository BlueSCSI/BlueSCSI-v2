// Copyright (c) 2022 Rabbit Hole Computing™
// Copyright (c) 2024 Tech by Androda, LLC

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
bool perform_parity_checking = true;

// Release bus and pulse RST signal, initialize PHY to host mode.
void scsiHostPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    SCSI_ENABLE_INITIATOR();

    scsi_accel_host_init();

    SCSI_OUT(RST, 1);
#ifndef LIB_FREERTOS_KERNEL
    delay(2);
#else
    // In FreeRTOS, this function runs before the schedule has started. We need to 
    // use the pico SDK delay instead of the task delay
    sleep_ms(2);
#endif
    SCSI_OUT(RST, 0);
#ifndef LIB_FREERTOS_KERNEL
    delay(250);
#else
    sleep_ms(250);
#endif
    g_scsiHostPhyReset = false;
}

// Select a device, id 0-7.
// Returns true if the target answers to selection request.
bool scsiHostPhySelect(int target_id, int initiator_id)
{
    SCSI_ENABLE_INITIATOR();
    SCSI_RELEASE_OUTPUTS();

    // We can't write individual data bus bits, so use a bit modified
    // arbitration scheme. We always yield to any other initiator on
    // the bus.
    scsiLogInitiatorPhaseChange(BUS_BUSY);
    SCSI_OUT(REQ, 0);
    SCSI_OUT(BSY, 1);
    for (int wait = 0; wait < 10; wait++)
    {
#ifndef LIB_FREERTOS_KERNEL
        delayMicroseconds(1);
#else
        vTaskDelay(1);
#endif

        if (SCSI_IN_DATA() != 0)
        {
            debuglog("scsiHostPhySelect: bus is busy");
            scsiLogInitiatorPhaseChange(BUS_FREE);
            SCSI_RELEASE_OUTPUTS();
            return false;
        }
    }

    // Selection phase
    scsiLogInitiatorPhaseChange(SELECTION);
    debuglog("------ SELECTING ", target_id, " with initiator ID ", (int)initiator_id);
    SCSI_OUT(SEL, 1);
    delayMicroseconds(5);
    SCSI_OUT_DATA((1 << target_id) | (1 << initiator_id));
    delayMicroseconds(5);
    SCSI_OUT(BSY, 0);

    // Wait for target to respond
    for (int wait = 0; wait < 2500; wait++)
    {
        delayMicroseconds(100);
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
    SCSI_ENABLE_INITIATOR();
    return true;
}

// Read the current communication phase as signaled by the target
int scsiHostPhyGetPhase()
{
    volatile static absolute_time_t last_online_time;

    if (g_scsiHostPhyReset)
    {
        // Reset request from watchdog timer
        scsiHostPhyRelease();
        return BUS_FREE;
    }

    volatile int phase = 0;
    volatile bool req_in = SCSI_IN(REQ);
    if (SCSI_IN(CD)) phase |= __scsiphase_cd;
    if (SCSI_IN(IO)) phase |= __scsiphase_io;
    if (SCSI_IN(MSG)) phase |= __scsiphase_msg;

    if (phase == 0 && absolute_time_diff_us(last_online_time, get_absolute_time()) > 100)
    {
        // BlueSCSI doesn't need to assert OUT_BSY to check whether the bus is in use
        delayMicroseconds(1);

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
static inline uint8_t scsiHostReadOneByte(int* parityError, uint32_t *parityResult)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    uint16_t r = SCSI_IN_DATA();
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_OUT(ACK, 0);

    if (parityError && r != (g_scsi_parity_lookup[r & 0xFF] ^ SCSI_IO_DATA_MASK))
    {
        *parityError = 1;
        *parityResult = (uint32_t)r;
    }

    return (uint8_t)r;
}

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
                log("scsiHostWrite: sent ", (int)i, " bytes, expected ", (int)count);
                return i;
            }
        }

        scsiHostWriteOneByte(data[i]);
    }

    return count;
}

uint32_t scsiHostRead(uint8_t *data, uint32_t count)
{
    int parityError = 0;
    uint32_t parityResult;
    uint32_t fullcount = count;

    int cd_start = SCSI_IN(CD);
    int msg_start = SCSI_IN(MSG);

    if ((count & 1) == 0 && ((uint32_t)data & 1) == 0)
    {
        // Even number of bytes, use accelerated routine
        count = scsi_accel_host_read(data, count, &parityError, &parityResult, &g_scsiHostPhyReset);
        if (parityError && perform_parity_checking) {
            log("Parity error in scsi_accel_host_read(): ", parityResult);
        } else {
            parityError = 0;
        }
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            while (!SCSI_IN(REQ))
            {
                if (g_scsiHostPhyReset || !SCSI_IN(IO) || SCSI_IN(CD) != cd_start || SCSI_IN(MSG) != msg_start)
                {
                    // Target switched out of DATA_IN mode
                    count = i;
                }
            }
            data[i] = scsiHostReadOneByte(&parityError, &parityResult);
            if (parityError && perform_parity_checking) {
                log("Parity error in scsiReadOneByte(): ", parityResult);
            } else {
                parityError = 0;
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
            log("scsiHostRead: received ", (int)count, " bytes, expected ", (int)fullcount);
        }

        return count;
    }
}

// Release all bus signals
void scsiHostPhyRelease()
{
    scsiLogInitiatorPhaseChange(BUS_FREE);
    SCSI_RELEASE_OUTPUTS();
    SCSI_RELEASE_DATA_REQ();
}

void setInitiatorModeParityCheck(bool checkParity) {
    perform_parity_checking = checkParity;
}
