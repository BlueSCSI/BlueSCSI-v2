// Implements the low level interface to SCSI bus
// Partially derived from scsiPhy.c from SCSI2SD-V6

#include "scsiPhy.h"
#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_log_trace.h"
#include "ZuluSCSI_config.h"

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
            uint8_t atn_flag = SCSI_IN(ATN) ? SCSI_STS_SELECTION_ATN : 0;
            g_scsi_sts_selection = SCSI_STS_SELECTION_SUCCEEDED | atn_flag | sel_id;
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
        SCSI_OUT(BSY, 1);

        // On RP2040 hardware the ATN signal is only available after OUT_BSY enables
        // the IO buffer U105, so check the signal status here.
        delay_100ns();
        scsiDev.atnFlag |= scsiStatusATN();
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
        azdbg("BUS RESET");
        scsiDev.resetFlag = 1;
    }
}

static void scsiPhyIRQ(uint gpio, uint32_t events)
{
    if (gpio == SCSI_IN_BSY)
    {
        // Note BSY interrupts only when we are not driving OUT_BSY low ourselves.
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

    // Enable BSY and RST interrupts
    // Note: RP2040 library currently supports only one callback,
    // so it has to be same for both pins.
    gpio_set_irq_enabled_with_callback(SCSI_IN_BSY, GPIO_IRQ_EDGE_RISE, true, scsiPhyIRQ);
    gpio_set_irq_enabled_with_callback(SCSI_IN_RST, GPIO_IRQ_EDGE_FALL, true, scsiPhyIRQ);
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
    // ANSI INCITS 362-2002 SPI-3 10.7.1:
    // Phase changes are not allowed while REQ or ACK is asserted.
    while (likely(!scsiDev.resetFlag) && SCSI_IN(ACK)) {}

    if (phase != g_scsi_phase)
    {
        int oldphase = g_scsi_phase;
        g_scsi_phase = (SCSI_PHASE)phase;
        scsiLogPhaseChange(phase);

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
    scsiLogDataIn(data, count);
    for (uint32_t i = 0; i < count; i++)
    {
        if (scsiDev.resetFlag) break;
        scsiWriteOneByte(data[i]);
    }
}

extern "C" void scsiStartWrite(const uint8_t* data, uint32_t count)
{
    // If the platform supports DMA for either SD card access or for SCSI bus,
    // this function can be used to execute SD card transfers in parallel with
    // SCSI transfers. This usually doubles the transfer speed.
    //
    // For simplicity, this example only implements blocking writes.
    scsiWrite(data, count);
}

extern "C" bool scsiIsWriteFinished(const uint8_t *data)
{
    // Asynchronous writes are not implemented in this example.
    return true;
}

extern "C" void scsiFinishWrite()
{
    // Asynchronous writes are not implemented in this example.
}

/*********************/
/* Receive from host */
/*********************/

// Read one byte from SCSI host using the handshake mechanism.
static inline uint8_t scsiReadOneByte(void)
{
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE(ACK);
    delay_100ns();
    uint8_t r = SCSI_IN_DATA();
    SCSI_OUT(REQ, 0);
    SCSI_WAIT_INACTIVE(ACK);

    return r;
}

extern "C" uint8_t scsiReadByte(void)
{
    uint8_t r = scsiReadOneByte();
    scsiLogDataOut(&r, 1);
    return r;
}

extern "C" void scsiRead(uint8_t* data, uint32_t count, int* parityError)
{
    *parityError = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        if (scsiDev.resetFlag) break;

        data[i] = scsiReadOneByte();
    }

    scsiLogDataOut(data, count);
}
