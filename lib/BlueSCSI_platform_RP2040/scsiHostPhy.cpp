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

// Release bus and pulse RST signal, initialize PHY to host mode.
void scsiHostPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    SCSI_ENABLE_INITIATOR();

    scsi_accel_host_init();

    SCSI_OUT(RST, 1);
    delay(2);
    SCSI_OUT(RST, 0);
    delay(250);
    g_scsiHostPhyReset = false;
}

// Select a device, id 0-7.
// Returns true if the target answers to selection request.
bool scsiHostPhySelect(int target_id)
{
    SCSI_RELEASE_OUTPUTS();

    // We can't write individual data bus bits, so use a bit modified
    // arbitration scheme. We always yield to any other initiator on
    // the bus.
    scsiLogInitiatorPhaseChange(BUS_BUSY);
    SCSI_OUT(BSY, 1);
    for (int wait = 0; wait < 10; wait++)
    {
        delayMicroseconds(1);

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
    debuglog("------ SELECTING ", target_id);
    SCSI_OUT(SEL, 1);
    delayMicroseconds(5);
    SCSI_OUT_DATA(1 << target_id);
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

    // We need to assert OUT_BSY to enable IO buffer U105 to read status signals.
    SCSI_RELEASE_DATA_REQ();
    SCSI_OUT(BSY, 1);
    SCSI_OUT(SEL, 0);
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
        // Disable OUT_BSY for a short time to see if the target is still on line
        SCSI_OUT(BSY, 0);
        delayMicroseconds(1);

        if (!SCSI_IN(BSY))
        {
            scsiLogInitiatorPhaseChange(BUS_FREE);
            return BUS_FREE;
        }

        // Still online, re-enable OUT_BSY to enable IO buffers
        SCSI_OUT(BSY, 1);
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
static inline uint8_t scsiHostReadOneByte(int* parityError)
{
    SCSIHOST_WAIT_ACTIVE(REQ);
    uint16_t r = SCSI_IN_DATA();
    SCSI_OUT(ACK, 1);
    SCSIHOST_WAIT_INACTIVE(REQ);
    SCSI_OUT(ACK, 0);

    if (parityError && r != (g_scsi_parity_lookup[r & 0xFF] ^ SCSI_IO_DATA_MASK))
    {
        log("Parity error in scsiReadOneByte(): ", (uint32_t)r);
        *parityError = 1;
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
    uint32_t fullcount = count;

    int cd_start = SCSI_IN(CD);
    int msg_start = SCSI_IN(MSG);

    if ((count & 1) == 0 && ((uint32_t)data & 1) == 0)
    {
        // Even number of bytes, use accelerated routine
        count = scsi_accel_host_read(data, count, &parityError, &g_scsiHostPhyReset);
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

            data[i] = scsiHostReadOneByte(&parityError);
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
}
