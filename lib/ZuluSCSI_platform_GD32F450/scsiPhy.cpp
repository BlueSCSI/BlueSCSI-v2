// Implements the low level interface to SCSI bus
// Partially derived from scsiPhy.c from SCSI2SD-V6

#include "scsiPhy.h"
#include "ZuluSCSI_platform.h"
#include "scsi_accel_asm.h"
#include "scsi_accel_dma.h"
#include "scsi_accel_sync.h"
#include "ZuluSCSI_log.h"
#include "ZuluSCSI_log_trace.h"
#include "ZuluSCSI_config.h"
#include <minIni.h>

#include <scsi2sd.h>
extern "C" {
#include <scsi.h>
#include <scsi2sd_time.h>
}

// Acceleration mode in use
static enum {
    PHY_MODE_BEST_AVAILABLE = 0,
    PHY_MODE_PIO = 1,
    PHY_MODE_DMA_TIMER = 2,
    PHY_MODE_GREENPAK_PIO = 3,
    PHY_MODE_GREENPAK_DMA = 4
} g_scsi_phy_mode;
static const char *g_scsi_phy_mode_names[] = {
    "Unknown", "PIO", "DMA_TIMER" // removing greenpak , "GREENPAK_PIO", "GREENPAK_DMA"
};

// State of polling write request
static struct {
    const uint8_t *data;
    uint32_t count;
    bool use_sync_mode;
} g_scsi_writereq;

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

static void selectPhyMode()
{
    int oldmode = g_scsi_phy_mode;

    int default_mode = PHY_MODE_BEST_AVAILABLE;

    // Read overriding setting from configuration file
    int wanted_mode = ini_getl("SCSI", "PhyMode", default_mode, CONFIGFILE);

    // Default: software GPIO bitbang, available on all revisions
    g_scsi_phy_mode = PHY_MODE_PIO;
    
    // Timer based DMA bitbang, available on V1.1, 2.8 MB/s
#ifdef SCSI_ACCEL_DMA_AVAILABLE
    if (wanted_mode == PHY_MODE_BEST_AVAILABLE || wanted_mode == PHY_MODE_DMA_TIMER)
    {
        g_scsi_phy_mode = PHY_MODE_DMA_TIMER;
    }
#endif

    // GreenPAK with software write, available on V1.1 with extra chip, 3.5 MB/s
    if (wanted_mode == PHY_MODE_BEST_AVAILABLE || wanted_mode == PHY_MODE_GREENPAK_PIO)
    {
        if (greenpak_is_ready())
        {
            g_scsi_phy_mode = PHY_MODE_GREENPAK_PIO;
        }
    }

    // GreenPAK with DMA write, available on V1.1 with extra chip
#ifdef SCSI_ACCEL_DMA_AVAILABLE
    if (wanted_mode == PHY_MODE_BEST_AVAILABLE || wanted_mode == PHY_MODE_GREENPAK_DMA)
    {
        if (greenpak_is_ready())
        {
            g_scsi_phy_mode = PHY_MODE_GREENPAK_DMA;
        }
    }
#endif

    if (g_scsi_phy_mode != oldmode)
    {
        azlog("SCSI PHY operating mode: ", g_scsi_phy_mode_names[g_scsi_phy_mode]);
    }
}

extern "C" void scsiPhyReset(void)
{
    SCSI_RELEASE_OUTPUTS();
    scsi_accel_dma_stopWrite();

    g_scsi_sts_selection = 0;
    g_scsi_ctrl_bsy = 0;
    g_scsi_writereq.count = 0;
    init_irqs();

#ifdef SCSI_SYNC_MODE_AVAILABLE
    scsi_accel_sync_init();
#endif

    selectPhyMode();

    if (g_scsi_phy_mode == PHY_MODE_DMA_TIMER)
    {
        scsi_accel_timer_dma_init();
    }
    else if (g_scsi_phy_mode == PHY_MODE_GREENPAK_DMA)
    {
        scsi_accel_greenpak_dma_init();
    }
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
            // after a command. The code in ZuluSCSI_disk.cpp tries to do this while waiting
            // for SD card, to avoid any extra latency.
            s2s_delay_ns(400000);
        }

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
    scsiStartWrite(data, count);
    scsiFinishWrite();
}

extern "C" void scsiStartWrite(const uint8_t* data, uint32_t count)
{
    scsiLogDataIn(data, count);

    g_scsi_writereq.use_sync_mode = (g_scsi_phase == DATA_IN && scsiDev.target->syncOffset > 0);

    if (g_scsi_phy_mode == PHY_MODE_PIO
        || g_scsi_phy_mode == PHY_MODE_GREENPAK_PIO
        || g_scsi_writereq.use_sync_mode)
    {
        // Software based bit-banging.
        // Write requests are queued and then executed in isWriteFinished() callback.
        // This allows better parallelism with SD card transfers.
        
        if (g_scsi_writereq.count)
        {
            if (data == g_scsi_writereq.data + g_scsi_writereq.count)
            {
                // Combine with previous one
                g_scsi_writereq.count += count;
                return;
            }
            else
            {
                // Actually execute previous request
                scsiFinishWrite();
            }
        }

        g_scsi_writereq.data = data;
        g_scsi_writereq.count = count;
    }
    else if (g_scsi_phy_mode == PHY_MODE_DMA_TIMER || g_scsi_phy_mode == PHY_MODE_GREENPAK_DMA)
    {
        // Accelerated writes using DMA and timers
        scsi_accel_dma_startWrite(data, count, &scsiDev.resetFlag);
    }
    else
    {
        azlog("Unknown SCSI PHY mode: ", (int)g_scsi_phy_mode);
    }
}

static void processPollingWrite(uint32_t count)
{
    if (count > g_scsi_writereq.count)
        count = g_scsi_writereq.count;
    
    const uint8_t *data = g_scsi_writereq.data;
    uint32_t count_words = count / 4;

    if (g_scsi_writereq.use_sync_mode)
    {
        // Synchronous mode transfer
        scsi_accel_sync_send(data, count, &scsiDev.resetFlag);
    }
    else if (count_words * 4 == count)
    {
        if (g_scsi_phy_mode == PHY_MODE_GREENPAK_PIO)
        {
            // GreenPAK PIO accelerated asynchronous transfer
            scsi_accel_greenpak_send((const uint32_t*)data, count_words, &scsiDev.resetFlag);
        }
        else
        {
            // Assembler optimized asynchronous transfer
            scsi_accel_asm_send((const uint32_t*)data, count_words, &scsiDev.resetFlag);
        }
    }
    else
    {
        // Use simple loop for unaligned transfers
        for (uint32_t i = 0; i < count; i++)
        {
            if (scsiDev.resetFlag) break;
            scsiWriteOneByte(data[i]);
        }
    }

    g_scsi_writereq.count -= count;
    if (g_scsi_writereq.count)
    {
        g_scsi_writereq.data += count;
    }
    else
    {
        g_scsi_writereq.data = NULL;
    }
}

static bool isPollingWriteFinished(const uint8_t *data)
{
    if (g_scsi_writereq.count)
    {
        if (data == NULL)
        {
            return false;
        }
        else if (data >= g_scsi_writereq.data &&
            data < g_scsi_writereq.data + g_scsi_writereq.count)
        {
            return false;
        }
    }
    return true;
}

extern "C" bool scsiIsWriteFinished(const uint8_t *data)
{
    // Check if there is still a polling transfer in progress
    if (!isPollingWriteFinished(data) && !check_sd_read_done())
    {
        // Process the transfer piece-by-piece while waiting
        // for SD card to react.
        int max_count = g_scsi_writereq.count / 8;
        
        // Always transfer whole sectors without pause to avoid problems with some SCSI hosts.
        int bytesPerSector = 512;
        if (scsiDev.target)
        {
            bytesPerSector = scsiDev.target->liveCfg.bytesPerSector;
        }
        if (max_count % bytesPerSector != 0) max_count -= (max_count % bytesPerSector);
        if (max_count < bytesPerSector) max_count = bytesPerSector;
        
        // Avoid SysTick interrupt pauses during the transfer
        SysTick_Handle_PreEmptively();

        processPollingWrite(max_count);
        return isPollingWriteFinished(data);
    }
    
    if (g_scsi_phy_mode == PHY_MODE_DMA_TIMER || g_scsi_phy_mode == PHY_MODE_GREENPAK_DMA)
    {
        return scsi_accel_dma_isWriteFinished(data);
    }
    else
    {
        return true;
    }
}

extern "C" void scsiFinishWrite()
{
    if (g_scsi_writereq.count)
    {
        // Finish previously started polling write request.
        processPollingWrite(g_scsi_writereq.count);
    }

    if (g_scsi_phy_mode == PHY_MODE_DMA_TIMER || g_scsi_phy_mode == PHY_MODE_GREENPAK_DMA)
    {
        scsi_accel_dma_finishWrite(&scsiDev.resetFlag);
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
    bool use_greenpak = (g_scsi_phy_mode == PHY_MODE_GREENPAK_DMA || g_scsi_phy_mode == PHY_MODE_GREENPAK_PIO);

    SysTick_Handle_PreEmptively();

    if (g_scsi_phase == DATA_OUT && scsiDev.target->syncOffset > 0)
    {
        // Synchronous data transfer
        scsi_accel_sync_recv(data, count, parityError, &scsiDev.resetFlag);
    }
    else if (count_words * 4 == count && count_words >= 2 && use_greenpak)
    {
        // GreenPAK accelerated receive can handle a multiple of 4 bytes with minimum of 8 bytes.
        scsi_accel_greenpak_recv((uint32_t*)data, count_words, &scsiDev.resetFlag);
    }
    else if (count_words * 4 == count && count_words >= 1)
    {
        // Optimized ASM subroutine can handle multiple of 4 bytes with minimum of 4 bytes.
        scsi_accel_asm_recv((uint32_t*)data, count_words, &scsiDev.resetFlag);
    }
    else
    {
        // Use a simple loop for short and unaligned transfers
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

    if (exti_interrupt_flag_get(SCSI_SEL_EXTI))
    {
        // Check BSY line status when SEL goes active.
        // This is needed to handle SCSI-1 hosts that use the single initiator mode.
        // The host will just assert the SEL directly, without asserting BSY first.
        exti_interrupt_flag_clear(SCSI_SEL_EXTI);
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

#if (SCSI_SEL_IRQn != SCSI_RST_IRQn) && (SCSI_SEL_IRQn != SCSI_BSY_IRQn)
extern "C"
void SCSI_SEL_IRQ (void)
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

    // Falling edge of SEL pin
    gpio_exti_source_select(SCSI_SEL_EXTI_SOURCE_PORT, SCSI_SEL_EXTI_SOURCE_PIN);
    exti_init(SCSI_SEL_EXTI, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    NVIC_SetPriority(SCSI_SEL_IRQn, 1);
    NVIC_EnableIRQ(SCSI_SEL_IRQn);
}


