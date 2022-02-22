// Implements the low level interface to SCSI bus
// Partially derived from scsiPhy.c from SCSI2SD-V6

#include "scsiPhy.h"
#include "AzulSCSI_platform.h"
#include "scsi_accel_asm.h"
#include "AzulSCSI_log.h"
#include "AzulSCSI_log_trace.h"

#include <scsi2sd.h>
extern "C" {
#include <scsi.h>
#include <time.h>
}

static void init_irqs();

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

static void scsi_bsy_deassert_interrupt()
{
    if (SCSI_IN(SEL) && !SCSI_IN(BSY))
    {
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
    }

    return SCSI_IN(SEL);
}

/************************/
/* SCSI bus reset logic */
/************************/

static void scsi_rst_assert_interrupt()
{
    bool rst1 = SCSI_IN(RST);
    delay_ns(500);
    bool rst2 = SCSI_IN(RST);

    if (rst1 && rst2)
    {
        azdbg("BUS RESET");
        scsiDev.resetFlag = 1;
    }
}

extern "C" void scsiPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    g_scsi_sts_selection = 0;
    g_scsi_ctrl_bsy = 0;
    init_irqs();
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

static inline void scsiWriteOneByte(uint8_t value)
{
    SCSI_OUT_DATA(value);
    delay_100ns(); // DB setup time before REQ
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE(ACK);
    SCSI_RELEASE_DATA_REQ(); // Release data and REQ
    SCSI_WAIT_INACTIVE(ACK);
}

extern "C" void scsiWriteByte(uint8_t value)
{
    scsiLogDataIn(&value, 1);
    scsiWriteOneByte(value);
}

extern "C" void scsiWrite(const uint8_t* data, uint32_t count)
{
    uint32_t count_words = count / 4;
    scsiLogDataIn(data, count);
    if (count_words * 4 == count)
    {
        // Use accelerated subroutine
        scsi_accel_asm_send((const uint32_t*)data, count_words, &scsiDev.resetFlag);
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            if (scsiDev.resetFlag) break;
            scsiWriteOneByte(data[i]);
        }
    }
}

/*********************/
/* Receive from host */
/*********************/

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

    uint32_t count_words = count / 4;
    if (count_words * 4 == count)
    {
        // Use accelerated subroutine
        scsi_accel_asm_recv((uint32_t*)data, count_words, &scsiDev.resetFlag);
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            if (scsiDev.resetFlag) break;

            data[i] = scsiReadOneByte();
        }
    }

    scsiLogDataOut(data, count);
}

/**********************/
/* Interrupt handlers */
/**********************/

extern "C"
void SCSI_RST_IRQ (void)
{
    if (exti_interrupt_flag_get(SCSI_RST_EXTI))
    {
        exti_interrupt_flag_clear(SCSI_RST_EXTI);
        scsi_rst_assert_interrupt();
    }

    if (exti_interrupt_flag_get(SCSI_BSY_EXTI))
    {
        exti_interrupt_flag_clear(SCSI_BSY_EXTI);
        scsi_bsy_deassert_interrupt();
    }
}

#if SCSI_RST_IRQn != SCSI_BSY_IRQn
extern "C"
void SCSI_BSY_IRQ (void)
{
    SCSI_RST_IRQ();
}
#endif

static void init_irqs()
{
    // Falling edge of RST pin
    gpio_exti_source_select(SCSI_RST_EXTI_SOURCE_PORT, SCSI_RST_EXTI_SOURCE_PIN);
    exti_init(SCSI_RST_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    NVIC_SetPriority(SCSI_RST_IRQn, 1);
    NVIC_EnableIRQ(SCSI_RST_IRQn);

    // Rising edge of BSY pin
    gpio_exti_source_select(SCSI_BSY_EXTI_SOURCE_PORT, SCSI_BSY_EXTI_SOURCE_PIN);
    exti_init(SCSI_BSY_EXTI, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    NVIC_SetPriority(SCSI_BSY_IRQn, 1);
    NVIC_EnableIRQ(SCSI_BSY_IRQn);
}


