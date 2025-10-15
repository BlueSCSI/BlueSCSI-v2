/** 
 * SCSI2SD V6 - Copyright (C) 2013 Michael McMaster <michael@codesrc.com>
 * ZuluSCSI™ - Copyright (c) 2022-2025 Rabbit Hole Computing™
 * 
 * This file is licensed under the GPL version 3 or any later version.  
 * It is derived from scsiPhy.c in SCSI2SD V6.
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

// Implements the low level interface to SCSI bus
// Partially derived from scsiPhy.c from SCSI2SD-V6

#include "scsiPhy.h"
#include "BlueSCSI_platform.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_log_trace.h"
#include "BlueSCSI_config.h"
#include "scsi_accel_target.h"
#include "hardware/structs/iobank0.h"

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

static SCSI_PHASE g_scsi_phase;
volatile uint8_t g_scsi_sts_selection;
volatile uint8_t g_scsi_ctrl_bsy;

void scsi_bsy_deassert_interrupt()
{
    if (SCSI_IN(SEL) && !SCSI_IN(BSY))
    {
        g_scsi_phase = BUS_BUSY;

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

#if !(defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE))
        SCSI_OUT(CD, 0);
        SCSI_OUT(MSG, 0);
        SCSI_ENABLE_CONTROL_OUT();
#endif

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
        dbgmsg("BUS RESET");
        scsiDev.resetFlag = 1;
    }
}

static void scsiPhyIRQ(uint gpio, uint32_t events)
{
    // dbgmsg("irq:", (uint32_t)gpio);
    if (gpio == SCSI_IN_BSY || gpio == SCSI_IN_SEL)
    {
        // Note BSY / SEL interrupts only when we are not driving OUT_BSY low ourselves.
        // The BSY input pin may be shared with other signals.
#if SCSI_OUT_BSY > 31
        if (sio_hw->gpio_hi_out & (1 << (SCSI_OUT_BSY - 32)))
#else
        if (sio_hw->gpio_out & (1 << SCSI_OUT_BSY))
#endif
        {
            scsi_bsy_deassert_interrupt();
        }
    }

#if (defined(BLUESCSI_ULTRA) || defined(BLUESCSI_ULTRA_WIDE))
    else if (gpio == SCSI_IN_RST)
#else
    else if (gpio == SCSI_IN_RST && ((~sio_hw->gpio_oe) & (1 << SCSI_OUT_SEL)))
#endif
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
    g_scsi_phase = BUS_FREE;


#ifndef RP2MCU_DISABLE_SCSI_ACCEL
    scsi_accel_rp2040_init();
#endif

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

static bool useWideMode()
{
    return (scsiDev.target->busWidth == 1) && (g_scsi_phase == DATA_IN || g_scsi_phase == DATA_OUT);
}

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

#ifndef RP2MCU_DISABLE_SCSI_ACCEL
        // Select between synchronous vs. asynchronous SCSI writes
        bool syncstatus = false;
        if (scsiDev.target->syncOffset > 0 && (g_scsi_phase == DATA_IN || g_scsi_phase == DATA_OUT))
        {
            syncstatus = scsi_accel_rp2040_setSyncMode(scsiDev.target->syncOffset, scsiDev.target->syncPeriod, useWideMode());
        }
        else
        {
            syncstatus = scsi_accel_rp2040_setSyncMode(0, 0, useWideMode());
        }

        if (!syncstatus)
        {
            // SCSI DMA was not idle, we are in some kind of error state, force bus reset
            scsiDev.resetFlag = 1;
            return 0;
        }
#endif

        if (phase < 0)
        {
            // Other communication on bus or reset state
            SCSI_RELEASE_OUTPUTS();
            return 0;
        }
        else
        {
            // The phase control signals should be changed close to simultaneously.
            // The SCSI spec allows 400 ns for this, but some hosts do not seem to be that
            // tolerant. The Cortex-M0 is also quite slow in bit twiddling.
            //
            // To avoid unnecessary delays, precalculate an XOR mask and then apply it
            // simultaneously to all three signals.
            uint32_t gpio_new = 0;
            if (!(phase & __scsiphase_msg)) { gpio_new |= (1 << SCSI_OUT_MSG); }
            if (!(phase & __scsiphase_cd)) { gpio_new |= (1 << SCSI_OUT_CD); }
            if (!(phase & __scsiphase_io)) { gpio_new |= (1 << SCSI_OUT_IO); }

            uint32_t mask = (1 << SCSI_OUT_MSG) | (1 << SCSI_OUT_CD) | (1 << SCSI_OUT_IO);
            uint32_t gpio_xor = (sio_hw->gpio_out ^ gpio_new) & mask;
            sio_hw->gpio_togl = gpio_xor;
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

// In synchronous mode the ACK pulse can be very short, so use edge IRQ to detect it.
#define CHECK_EDGE(pin) \
    ((iobank0_hw->intr[pin / 8] >> (4 * (pin % 8))) & GPIO_IRQ_EDGE_FALL)

#define SCSI_WAIT_ACTIVE_EDGE(pin) \
  if (!CHECK_EDGE(SCSI_IN_ ## pin)) { \
    while(!SCSI_IN(pin) && !CHECK_EDGE(SCSI_IN_ ## pin) && !scsiDev.resetFlag); \
  }

#define SCSI_WAIT_INACTIVE(pin) \
  if (SCSI_IN(pin)) { \
    if (SCSI_IN(pin)) { \
      while(SCSI_IN(pin) && !scsiDev.resetFlag); \
    } \
  }

// Write one byte or halfword to SCSI host using the handshake mechanism
// This is suitable for both asynchronous and synchronous communication.
static inline void scsiWriteOneByte(uint16_t value)
{
    SCSI_OUT_DATA(value);
    delay_100ns(); // DB setup time before REQ
    gpio_acknowledge_irq(SCSI_IN_ACK, GPIO_IRQ_EDGE_FALL);
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE_EDGE(ACK);
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
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
    if (useWideMode())
    {
        // 16-bit bus width
        uint32_t i = 0;
        while (i < count)
        {
            uint16_t w = data[i++];
            if (i < count) w |= (uint16_t)data[i++] << 8;
            scsiWriteOneByte(w);
        }
    }
    else
    {
        // 8-bit bus width
        for (uint32_t i = 0; i < count; i++)
        {
            scsiWriteOneByte(data[i]);
        }
    }
#else
    scsi_accel_rp2040_startWrite(data, count, &scsiDev.resetFlag);
#endif
}

extern "C" bool scsiIsWriteFinished(const uint8_t *data)
{
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
    return true;
#else
    return scsi_accel_rp2040_isWriteFinished(data);
#endif
}

extern "C" void scsiFinishWrite()
{
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
#else
    scsi_accel_rp2040_finishWrite(&scsiDev.resetFlag);
#endif
}

/*********************/
/* Receive from host */
/*********************/

// Read one byte or halfword from SCSI host using the handshake mechanism.
static inline uint16_t scsiReadOneByte(int* parityError)
{
    SCSI_OUT(REQ, 1);
    SCSI_WAIT_ACTIVE(ACK);
    delay_100ns();
    uint16_t r = SCSI_IN_DATA();
    SCSI_OUT(REQ, 0);
    SCSI_WAIT_INACTIVE(ACK);

    if (parityError && r != (g_scsi_parity_lookup[r & 0xFF] ^ (SCSI_IO_DATA_MASK >> SCSI_IO_SHIFT)))
    {
        logmsg("Parity error in scsiReadOneByte(): ", (uint32_t)r);
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
    if (!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY)) { parityError = NULL; }

    scsiStartRead(data, count, parityError);
    scsiFinishRead(data, count, parityError);
}

extern "C" void scsiStartRead(uint8_t* data, uint32_t count, int *parityError)
{
    if (!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY)) { parityError = NULL; }
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
    if (useWideMode())
    {
        // 16-bit bus width
        uint32_t i = 0;
        while (i < count)
        {
            uint16_t r = scsiReadOneByte(parityError);
            data[i++] = r & 0xFF;
            if (i < count) data[i++] = r >> 8;
        }
    }
    else
    {
        // 8-bit bus width
        for (uint32_t i = 0; i < count; i++)
        {
            data[i] = scsiReadOneByte(parityError);
        }
    }
#else
    scsi_accel_rp2040_startRead(data, count, parityError, &scsiDev.resetFlag);
#endif
}

extern "C" void scsiFinishRead(uint8_t* data, uint32_t count, int *parityError)
{
    if (!(scsiDev.boardCfg.flags & S2S_CFG_ENABLE_PARITY)) { parityError = NULL; }
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
#else
    scsi_accel_rp2040_finishRead(data, count, parityError, &scsiDev.resetFlag);
#endif
    scsiLogDataOut(data, count);
}

extern "C" bool scsiIsReadFinished(const uint8_t *data)
{
#ifdef RP2MCU_DISABLE_SCSI_ACCEL
    return true;
#else
    return scsi_accel_rp2040_isReadFinished(data);
#endif
}
