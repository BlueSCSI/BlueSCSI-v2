// Implements the low level interface to SCSI bus
// Partially derived from scsiPhy.c from SCSI2SD-V6

#include "scsiPhy.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_config.h"
#include "scsi_accel_rp2040.h"

#include <scsi2sd.h>
extern "C" {
#include <scsi.h>
#include <scsi2sd_time.h>
}

/***********************/
/* SCSI status signals */
/***********************/

extern "C" bool scsiStatusATN()
{
    return SCSI_IN(ATN);
}

extern "C" bool scsiStatusBSY()
{
    return SCSI_IN(BSY);
}

/************************/
/* SCSI selection logic */
/************************/

volatile uint8_t g_scsi_sts_selection;
volatile uint8_t g_scsi_ctrl_bsy;

void scsi_bsy_deassert_interrupt()
{
    if (SCSI_IN(SEL) && !SCSI_IN(BSY))
    {
        // Check if any of the targets we simulate is selected
        uint8_t sel_bits = SCSI_IN_DATA();
        int sel_id = -1;
        for (int i = 0; i < S2S_MAX_TARGETS; i++)
        {
            if (scsiDev.targets[i].targetId <= 7 && scsiDev.targets[i].cfg)
            {
                if (sel_bits & (1 << scsiDev.targets[i].targetId))
                {
                    sel_id = scsiDev.targets[i].targetId;
                    break;
                }
            }
        }

        if (sel_id >= 0)
        {
            // Set ATN flag here unconditionally, real value is only known after
            // OUT_BSY is enabled in scsiStatusSEL() below.
            g_scsi_sts_selection = SCSI_STS_SELECTION_SUCCEEDED | SCSI_STS_SELECTION_ATN | sel_id;
        }

        // selFlag is required for Philips P2000C which releases it after 600ns
        // without waiting for BSY.
        // Also required for some early Mac Plus roms
        scsiDev.selFlag = *SCSI_STS_SELECTED;
    }
}

extern "C" bool scsiStatusSEL()
{
    if (g_scsi_ctrl_bsy)
    {
        // We don't have direct register access to BSY bit like SCSI2SD scsi.c expects.
        // Instead update the state here.
        // Releasing happens with bus release.
        g_scsi_ctrl_bsy = 0;
        SCSI_OUT(CD, 0);
        SCSI_OUT(MSG, 0);
        SCSI_ENABLE_CONTROL_OUT();
        SCSI_OUT(BSY, 1);

        // On RP2040 hardware the ATN signal is only available after OUT_BSY enables
        // the IO buffer U105, so check the signal status here.
        delay_100ns();
        if (!scsiStatusATN())
        {
            // This is a SCSI1 host that does send IDENTIFY message
            scsiDev.atnFlag = 0;
            scsiDev.target->unitAttention = 0;
            scsiDev.compatMode = COMPAT_SCSI1;
        }
    }

    return SCSI_IN(SEL);
}

/************************/
/* SCSI bus reset logic */
/************************/

static void scsi_rst_assert_interrupt()
{
    // Glitch filtering
    bool rst1 = SCSI_IN(RST);
    delay_ns(500);
    bool rst2 = SCSI_IN(RST);

    if (rst1 && rst2)
    {
        bluedbg("BUS RESET");
        scsiDev.resetFlag = 1;
    }
}

static void scsiPhyIRQ(uint gpio, uint32_t events)
{
    if (gpio == SCSI_IN_BSY || gpio == SCSI_IN_SEL)
    {
        // Note BSY / SEL interrupts only when we are not driving OUT_BSY low ourselves.
        // The BSY input pin may be shared with other signals.
        if (sio_hw->gpio_out & (1 << SCSI_OUT_BSY))
        {
            scsi_bsy_deassert_interrupt();
        }
    }
    else if (gpio == SCSI_IN_RST)
    {
        scsi_rst_assert_interrupt();
    }
}

// This function is called to initialize the phy code.
// It is called after power-on and after SCSI bus reset.
extern "C" void scsiPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    g_scsi_sts_selection = 0;
    g_scsi_ctrl_bsy = 0;

    scsi_accel_rp2040_init();

    // Enable BSY, RST and SEL interrupts
    // Note: RP2040 library currently supports only one callback,
    // so it has to be same for both pins.
    gpio_set_irq_enabled_with_callback(SCSI_IN_BSY, GPIO_IRQ_EDGE_RISE, true, scsiPhyIRQ);
    gpio_set_irq_enabled(SCSI_IN_RST, GPIO_IRQ_EDGE_FALL, true);

    // Check BSY line status when SEL goes active.
    // This is needed to handle SCSI-1 hosts that use the single initiator mode.
    // The host will just assert the SEL directly, without asserting BSY first.
    gpio_set_irq_enabled(SCSI_IN_SEL, GPIO_IRQ_EDGE_FALL, true);
}

/************************/
/* SCSI bus phase logic */
/************************/

static SCSI_PHASE g_scsi_phase;

extern "C" void scsiEnterPhase(int phase)
{
    int delay = scsiEnterPhaseImmediate(phase);
    if (delay > 0)
    {
        s2s_delay_ns(delay);
    }
}

// Change state and return nanosecond delay to wait
extern "C" uint32_t scsiEnterPhaseImmediate(int phase)
{
    if (phase != g_scsi_phase)
    {
        // ANSI INCITS 362-2002 SPI-3 10.7.1:
        // Phase changes are not allowed while REQ or ACK is asserted.
        while (likely(!scsiDev.resetFlag) && SCSI_IN(ACK)) {}

        if (scsiDev.compatMode < COMPAT_SCSI2 && (phase == DATA_IN || phase == DATA_OUT))
        {
            // Akai S1000/S3000 seems to need extra delay before changing to data phase
            // after a command. The code in BlueSCSI_disk.cpp tries to do this while waiting
            // for SD card, to avoid any extra latency.
            s2s_delay_ns(400000);
        }

        int oldphase = g_scsi_phase;
        g_scsi_phase = (SCSI_PHASE)phase;
        scsiLogPhaseChange(phase);

        // Select between synchronous vs. asynchronous SCSI writes
        if (g_scsi_phase == DATA_IN && scsiDev.target->syncOffset > 0)
        {
            scsi_accel_rp2040_setWriteMode(scsiDev.target->syncOffset, scsiDev.target->syncPeriod);
        }
        else
        {
            scsi_accel_rp2040_setWriteMode(0, 0);
        }

        if (phase < 0)
        {
            // Other communication on bus or reset state
            SCSI_RELEASE_OUTPUTS();
            return 0;
        }
        else
        {
            SCSI_OUT(MSG, phase & __scsiphase_msg);
            SCSI_OUT(CD,  phase & __scsiphase_cd);
            SCSI_OUT(IO,  phase & __scsiphase_io);
            SCSI_ENABLE_CONTROL_OUT();

            int delayNs = 400; // Bus settle delay
            if ((oldphase & __scsiphase_io) != (phase & __scsiphase_io))
            {
                delayNs += 400; // Data release delay
            }

            if (scsiDev.compatMode < COMPAT_SCSI2)
            {
                // EMU EMAX needs 100uS ! 10uS is not enough.
                delayNs += 100000;
            }

            return delayNs;
        }
    }
    else
    {
        return 0;
    }
}

// Release all signals
void scsiEnterBusFree(void)
{
    g_scsi_phase = BUS_FREE;
    g_scsi_sts_selection = 0;
    g_scsi_ctrl_bsy = 0;
    scsiDev.cdbLen = 0;

    SCSI_RELEASE_OUTPUTS();
}

/********************/
/* Transmit to host */
/********************/

#define SCSI_WAIT_ACTIVE(pin) \
  if (!SCSI_IN(pin)) { \
    if (!SCSI_IN(pin)) { \
      while(!SCSI_IN(pin) && !scsiDev.resetFlag); \
    } \
  }

#define SCSI_WAIT_INACTIVE(pin) \
  if (SCSI_IN(pin)) { \
    if (SCSI_IN(pin)) { \
      while(SCSI_IN(pin) && !scsiDev.resetFlag); \
    } \
  }

// Write one byte to SCSI host using the handshake mechanism
static inline void scsiWriteOneByte(uint8_t value)
{
    SCSI_OUT_DATA(value);
    delay_100ns(); // DB setup time before REQ
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE(ACK);
    SCSI_RELEASE_DATA_REQ();
    SCSI_WAIT_INACTIVE(ACK);
}

extern "C" void scsiWriteByte(uint8_t value)
{
    scsiLogDataIn(&value, 1);
    scsiWriteOneByte(value);
}

extern "C" void scsiWrite(const uint8_t* data, uint32_t count)
{
    scsiStartWrite(data, count);
    scsiFinishWrite();
}

extern "C" void scsiStartWrite(const uint8_t* data, uint32_t count)
{
    scsiLogDataIn(data, count);

    if ((count & 1) != 0 || ((uint32_t)data & 1) != 0)
    {
        // Unaligned write, do it byte-by-byte
        scsiFinishWrite();
        for (uint32_t i = 0; i < count; i++)
        {
            if (scsiDev.resetFlag) break;
            scsiWriteOneByte(data[i]);
        }
    }
    else
    {
        // Use accelerated routine
        scsi_accel_rp2040_startWrite(data, count, &scsiDev.resetFlag);
    }
}

extern "C" bool scsiIsWriteFinished(const uint8_t *data)
{
    return scsi_accel_rp2040_isWriteFinished(data);
}

extern "C" void scsiFinishWrite()
{
    scsi_accel_rp2040_finishWrite(&scsiDev.resetFlag);
}

/*********************/
/* Receive from host */
/*********************/

// Read one byte from SCSI host using the handshake mechanism.
static inline uint8_t scsiReadOneByte(int* parityError)
{
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE(ACK);
    delay_100ns();
    uint16_t r = SCSI_IN_DATA();
    SCSI_OUT(REQ, 0);
    SCSI_WAIT_INACTIVE(ACK);

    if (parityError && r != (g_scsi_parity_lookup[r & 0xFF] ^ SCSI_IO_DATA_MASK))
    {
        bluelog("Parity error in scsiReadOneByte(): ", (uint32_t)r);
        *parityError = 1;
    }

    return (uint8_t)r;
}

extern "C" uint8_t scsiReadByte(void)
{
    uint8_t r = scsiReadOneByte(NULL);
    scsiLogDataOut(&r, 1);
    return r;
}

extern "C" void scsiRead(uint8_t* data, uint32_t count, int* parityError)
{
    *parityError = 0;

    if ((count & 1) != 0 || ((uint32_t)data & 1) != 0)
    {
        // Unaligned transfer, do byte by byte
        for (uint32_t i = 0; i < count; i++)
        {
            if (scsiDev.resetFlag) break;
            data[i] = scsiReadOneByte(parityError);
        }
    }
    else
    {
        // Use accelerated routine
        scsi_accel_rp2040_read(data, count, parityError, &scsiDev.resetFlag);
    }

    scsiLogDataOut(data, count);
}
