/** 
 * Copyright (C) 2023 saybur
 * Copyright (C) 2025 Tech by Androda, LLC
 * 
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

#ifdef ENABLE_AUDIO_OUTPUT_SPDIF

#include <SdFat.h>
#include <stdbool.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <pico/multicore.h>
#include "audio_spdif.h"
#include "BlueSCSI_audio.h"
#include "BlueSCSI_config.h"
#include "BlueSCSI_log.h"
#include "BlueSCSI_platform.h"
#include "shift.pio.h"
#include "timings_RP2MCU.h"

extern SdFs SD;

// Table with the number of '1' bits for each index.
// Used for S/PDIF parity calculations.
// Placed in SRAM5 for the second core to use with reduced contention.
const uint8_t snd_parity[256] __attribute__((aligned(256), section(".scratch_y.snd_parity"))) = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8, };

/*
 * Precomputed biphase-mark patterns for data. For an 8-bit value this has
 * 16-bits in MSB-first order for the correct high/low transitions to
 * represent the data, given an output clocking rate twice the bitrate (so the
 * bits '11' or '00' reflect a zero and '10' or '01' represent a one). Each
 * value below starts with a '1' and will need to be inverted if the last bit
 * of the previous mask was also a '1'. These values can be written to an
 * appropriately configured SPI peripheral to blast biphase data at a
 * receiver.
 * 
 * To facilitate fast lookups this table should be put in SRAM with low
 * contention, aligned to an apppropriate boundry.
 */
const uint16_t biphase[256] __attribute__((aligned(512), section(".scratch_y.biphase"))) = {
    0xCCCC, 0xB333, 0xD333, 0xACCC, 0xCB33, 0xB4CC, 0xD4CC, 0xAB33,
    0xCD33, 0xB2CC, 0xD2CC, 0xAD33, 0xCACC, 0xB533, 0xD533, 0xAACC,
    0xCCB3, 0xB34C, 0xD34C, 0xACB3, 0xCB4C, 0xB4B3, 0xD4B3, 0xAB4C,
    0xCD4C, 0xB2B3, 0xD2B3, 0xAD4C, 0xCAB3, 0xB54C, 0xD54C, 0xAAB3,
    0xCCD3, 0xB32C, 0xD32C, 0xACD3, 0xCB2C, 0xB4D3, 0xD4D3, 0xAB2C,
    0xCD2C, 0xB2D3, 0xD2D3, 0xAD2C, 0xCAD3, 0xB52C, 0xD52C, 0xAAD3,
    0xCCAC, 0xB353, 0xD353, 0xACAC, 0xCB53, 0xB4AC, 0xD4AC, 0xAB53,
    0xCD53, 0xB2AC, 0xD2AC, 0xAD53, 0xCAAC, 0xB553, 0xD553, 0xAAAC,
    0xCCCB, 0xB334, 0xD334, 0xACCB, 0xCB34, 0xB4CB, 0xD4CB, 0xAB34,
    0xCD34, 0xB2CB, 0xD2CB, 0xAD34, 0xCACB, 0xB534, 0xD534, 0xAACB,
    0xCCB4, 0xB34B, 0xD34B, 0xACB4, 0xCB4B, 0xB4B4, 0xD4B4, 0xAB4B,
    0xCD4B, 0xB2B4, 0xD2B4, 0xAD4B, 0xCAB4, 0xB54B, 0xD54B, 0xAAB4,
    0xCCD4, 0xB32B, 0xD32B, 0xACD4, 0xCB2B, 0xB4D4, 0xD4D4, 0xAB2B,
    0xCD2B, 0xB2D4, 0xD2D4, 0xAD2B, 0xCAD4, 0xB52B, 0xD52B, 0xAAD4,
    0xCCAB, 0xB354, 0xD354, 0xACAB, 0xCB54, 0xB4AB, 0xD4AB, 0xAB54,
    0xCD54, 0xB2AB, 0xD2AB, 0xAD54, 0xCAAB, 0xB554, 0xD554, 0xAAAB,
    0xCCCD, 0xB332, 0xD332, 0xACCD, 0xCB32, 0xB4CD, 0xD4CD, 0xAB32,
    0xCD32, 0xB2CD, 0xD2CD, 0xAD32, 0xCACD, 0xB532, 0xD532, 0xAACD,
    0xCCB2, 0xB34D, 0xD34D, 0xACB2, 0xCB4D, 0xB4B2, 0xD4B2, 0xAB4D,
    0xCD4D, 0xB2B2, 0xD2B2, 0xAD4D, 0xCAB2, 0xB54D, 0xD54D, 0xAAB2,
    0xCCD2, 0xB32D, 0xD32D, 0xACD2, 0xCB2D, 0xB4D2, 0xD4D2, 0xAB2D,
    0xCD2D, 0xB2D2, 0xD2D2, 0xAD2D, 0xCAD2, 0xB52D, 0xD52D, 0xAAD2,
    0xCCAD, 0xB352, 0xD352, 0xACAD, 0xCB52, 0xB4AD, 0xD4AD, 0xAB52,
    0xCD52, 0xB2AD, 0xD2AD, 0xAD52, 0xCAAD, 0xB552, 0xD552, 0xAAAD,
    0xCCCA, 0xB335, 0xD335, 0xACCA, 0xCB35, 0xB4CA, 0xD4CA, 0xAB35,
    0xCD35, 0xB2CA, 0xD2CA, 0xAD35, 0xCACA, 0xB535, 0xD535, 0xAACA,
    0xCCB5, 0xB34A, 0xD34A, 0xACB5, 0xCB4A, 0xB4B5, 0xD4B5, 0xAB4A,
    0xCD4A, 0xB2B5, 0xD2B5, 0xAD4A, 0xCAB5, 0xB54A, 0xD54A, 0xAAB5,
    0xCCD5, 0xB32A, 0xD32A, 0xACD5, 0xCB2A, 0xB4D5, 0xD4D5, 0xAB2A,
    0xCD2A, 0xB2D5, 0xD2D5, 0xAD2A, 0xCAD5, 0xB52A, 0xD52A, 0xAAD5,
    0xCCAA, 0xB355, 0xD355, 0xACAA, 0xCB55, 0xB4AA, 0xD4AA, 0xAB55,
    0xCD55, 0xB2AA, 0xD2AA, 0xAD55, 0xCAAA, 0xB555, 0xD555, 0xAAAA };
/*
 * Biphase frame headers for S/PDIF, including the special bit framing
 * errors used to detect (sub)frame start conditions. See above table
 * for details.
 */
const uint16_t x_preamble = 0xE2CC;
const uint16_t y_preamble = 0xE4CC;
const uint16_t z_preamble = 0xE8CC;

// DMA configuration info
static dma_channel_config snd_dma_a_cfg;
static dma_channel_config snd_dma_b_cfg;

// some chonky buffers to store audio samples
static uint8_t sample_buf_a[AUDIO_BUFFER_SIZE];
static uint8_t sample_buf_b[AUDIO_BUFFER_SIZE];

// tracking for the state of the above buffers
enum bufstate { STALE, FILLING, READY };
static volatile bufstate sbufst_a = STALE;
static volatile bufstate sbufst_b = STALE;
enum bufselect { A, B };
static bufselect sbufsel = A;
static uint16_t sbufpos = 0;
static uint8_t sbufswap = 0;

// buffers for storing biphase patterns
#define SAMPLE_CHUNK_SIZE 1024 // ~5.8ms
#define WIRE_BUFFER_SIZE (SAMPLE_CHUNK_SIZE * 2)
static uint16_t wire_buf_a[WIRE_BUFFER_SIZE];
static uint16_t wire_buf_b[WIRE_BUFFER_SIZE];

// tracking for audio playback
static uint8_t audio_owner; // SCSI ID or 0xFF when idle
static volatile bool audio_paused = false;
static ImageBackingStore* audio_file;
static uint64_t fpos;
static uint32_t fleft;

// historical playback status information
static audio_status_code audio_last_status[8] = {ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS,
                                                 ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS};
// volume information for targets
static volatile uint16_t volumes[8] = {
    DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH,
    DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH, DEFAULT_VOLUME_LEVEL_2CH
};
static volatile uint16_t channels[8] = {
    AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK,
    AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK
};

// mechanism for cleanly stopping DMA units
static volatile bool audio_stopping = false;

// trackers for the below function call
static uint16_t sfcnt = 0; // sub-frame count; 2 per frame, 192 frames/block
static uint8_t invert = 0; // biphase encode help: set if last wire bit was '1'

// PIO Audio
#define SPDIF_PIO_UNIT pio1
static uint32_t spdif_pio_sm = 2;
static bool already_claimed = false;
static bool audio_setup_failed = false;
/*
 * Translates 16-bit stereo sound samples to biphase wire patterns for the
 * SPI peripheral. Produces 8 patterns (128 bits, or 1 S/PDIF frame) per pair
 * of input samples. Provided length is the total number of sample bytes present,
 * _twice_ the number of samples (little-endian order assumed)
 * 
 * This function operates with side-effects and is not safe to call from both
 * cores. It must also be called in the same order data is intended to be
 * output.
 */
static void snd_encode(uint8_t* samples, uint16_t* wire_patterns, uint16_t len, uint8_t swap) {
    uint16_t wvol = volumes[audio_owner & 7];
    uint8_t lvol = ((wvol >> 8) + (wvol & 0xFF)) >> 1; // average of both values
    // limit maximum volume; with my DACs I've had persistent issues
    // with signal clipping when sending data in the highest bit position
    lvol = lvol >> 2;
    uint8_t rvol = lvol;
    // enable or disable based on the channel information for both output
    // ports, where the high byte and mask control the right channel, and
    // the low control the left channel
    uint16_t chn = channels[audio_owner & 7] & AUDIO_CHANNEL_ENABLE_MASK;
    if (!(chn >> 8)) rvol = 0;
    if (!(chn & 0xFF)) lvol = 0;

    uint16_t widx = 0;
    for (uint16_t i = 0; i < len; i += 2) {
        uint32_t sample = 0;
        uint8_t parity = 0;
        if (samples != NULL) {
            int32_t rsamp;
            if (swap) {
                rsamp = (int16_t)(samples[i + 1] + (samples[i] << 8));
            } else {
                rsamp = (int16_t)(samples[i] + (samples[i + 1] << 8));
            }
            // linear scale to requested audio value
            if (i & 2) {
                rsamp *= rvol;
            } else {
                rsamp *= lvol;
            }
            // use 20 bits of value only, which allows ignoring the lowest 8
            // bits during biphase conversion (after including sample shift)
            sample = ((uint32_t)rsamp) & 0xFFFFF0;

            // determine parity, simplified to one lookup via XOR
            parity = ((sample >> 16) ^ (sample >> 8)) ^ sample;
            parity = snd_parity[parity];

            // shift sample into the correct bit positions of the sub-frame.
            sample = sample << 4;
        }

        // if needed, establish even parity with P bit
        if (parity % 2) sample |= 0x80000000;

        // translate sample into biphase encoding
        // first is low 8 bits: preamble and 4 least-significant bits of 
        // 24-bit audio, pre-encoded as all '0' due to 16-bit samples
        uint16_t wp;
        if (sfcnt == 0) {
            wp = z_preamble; // left channel, block start
        } else if (sfcnt % 2) {
            wp = y_preamble; // right channel
        } else {
            wp = x_preamble; // left channel, not block start
        }
        if (invert) wp = ~wp;
        invert = wp & 1;
        wire_patterns[widx++] = wp;
        // next 8 bits
        wp = biphase[(uint8_t) (sample >> 8)];
        if (invert) wp = ~wp;
        invert = wp & 1;
        wire_patterns[widx++] = wp;
        // next 8 again, all audio data
        wp = biphase[(uint8_t) (sample >> 16)];
        if (invert) wp = ~wp;
        invert = wp & 1;
        wire_patterns[widx++] = wp;
        // final 8, low 4 audio data and high 4 control bits
        wp = biphase[(uint8_t) (sample >> 24)];
        if (invert) wp = ~wp;
        invert = wp & 1;
        wire_patterns[widx++] = wp;
        // increment subframe counter for next pass
        sfcnt++;
        if (sfcnt == 384) sfcnt = 0; // if true, block complete
    }
}

// functions for passing to Core1
static void snd_process_a() {
    if (sbufsel == A) {
        if (sbufst_a == READY) {
            snd_encode(sample_buf_a + sbufpos, wire_buf_a, SAMPLE_CHUNK_SIZE, sbufswap);
            sbufpos += SAMPLE_CHUNK_SIZE;
            if (sbufpos >= AUDIO_BUFFER_SIZE) {
                sbufsel = B;
                sbufpos = 0;
                sbufst_a = STALE;
            }
        } else {
            snd_encode(NULL, wire_buf_a, SAMPLE_CHUNK_SIZE, sbufswap);
        }
    } else {
        if (sbufst_b == READY) {
            snd_encode(sample_buf_b + sbufpos, wire_buf_a, SAMPLE_CHUNK_SIZE, sbufswap);
            sbufpos += SAMPLE_CHUNK_SIZE;
            if (sbufpos >= AUDIO_BUFFER_SIZE) {
                sbufsel = A;
                sbufpos = 0;
                sbufst_b = STALE;
            }
        } else {
            snd_encode(NULL, wire_buf_a, SAMPLE_CHUNK_SIZE, sbufswap);
        }
    }
}
static void snd_process_b() {
    // clone of above for the other wire buffer
    if (sbufsel == A) {
        if (sbufst_a == READY) {
            snd_encode(sample_buf_a + sbufpos, wire_buf_b, SAMPLE_CHUNK_SIZE, sbufswap);
            sbufpos += SAMPLE_CHUNK_SIZE;
            if (sbufpos >= AUDIO_BUFFER_SIZE) {
                sbufsel = B;
                sbufpos = 0;
                sbufst_a = STALE;
            }
        } else {
            snd_encode(NULL, wire_buf_b, SAMPLE_CHUNK_SIZE, sbufswap);
        }
    } else {
        if (sbufst_b == READY) {
            snd_encode(sample_buf_b + sbufpos, wire_buf_b, SAMPLE_CHUNK_SIZE, sbufswap);
            sbufpos += SAMPLE_CHUNK_SIZE;
            if (sbufpos >= AUDIO_BUFFER_SIZE) {
                sbufsel = A;
                sbufpos = 0;
                sbufst_b = STALE;
            }
        } else {
            snd_encode(NULL, wire_buf_b, SAMPLE_CHUNK_SIZE, sbufswap);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* ---------- VISIBLE FUNCTIONS ------------------------------------------- */
/* ------------------------------------------------------------------------ */

void audio_dma_irq() {
    if (dma_hw->intr & (1 << SOUND_DMA_CHA)) {
        dma_hw->ints0 = (1 << SOUND_DMA_CHA);
        multicore_fifo_push_blocking((uintptr_t) &snd_process_a);
        if (audio_stopping) {
            channel_config_set_chain_to(&snd_dma_a_cfg, SOUND_DMA_CHA);
        }
        dma_channel_configure(SOUND_DMA_CHA,
                &snd_dma_a_cfg,
                &SPDIF_PIO_UNIT->txf[spdif_pio_sm],
                &wire_buf_a,
                WIRE_BUFFER_SIZE,
                false);
    } else if (dma_hw->intr & (1 << SOUND_DMA_CHB)) {
        dma_hw->ints0 = (1 << SOUND_DMA_CHB);
        multicore_fifo_push_blocking((uintptr_t) &snd_process_b);
        if (audio_stopping) {
            channel_config_set_chain_to(&snd_dma_b_cfg, SOUND_DMA_CHB);
        }
        dma_channel_configure(SOUND_DMA_CHB,
                &snd_dma_b_cfg,
                &SPDIF_PIO_UNIT->txf[spdif_pio_sm],
                &wire_buf_b,
                WIRE_BUFFER_SIZE,
                false);
    }
}

bool audio_is_active() {
    return audio_owner != 0xFF && g_scsi_settings.getSystem()->enableCDAudio;
}

bool audio_is_playing(uint8_t id) {
    return audio_owner == (id & 7);
}

void audio_setup() {
    if (!g_scsi_settings.getSystem()->enableCDAudio) {
        logmsg("Audio setup skipped, this build does not support CD Audio");
        audio_setup_failed = true;
        return;
    }
    if (g_bluescsi_timings->clk_hz != 203200000) {
        logmsg("!!! SPDIF CONFIGURATION MISMATCH !!!");
        logmsg("SPDIF Audio output only works at 203MHz CPU speed setting");
        logmsg("For SPDIF Audio to work, remove the SpeedGrade setting from BlueSCSI.ini");
        logmsg("SPDIF Audio Output setup failed");
        audio_setup_failed = true;
        return;
    }
#ifdef BLUESCSI_MCU_RP20XX
    logmsg("BlueSCSI CD Audio Enabled - Use SPDIF on I2C SCL pin");
#else
    logmsg("BlueSCSI CD Audio Enabled - Connect DAC to BlueSCSI or use SPDIF on I2C SCL pin");
#endif

    // Calculate clock divider, rounding up as necessary
    double clkdiv = ((double)(g_bluescsi_timings->clk_hz) / (double)(5644800));

    // Initialize the PIO unit and program
    if (pio_sm_is_claimed(SPDIF_PIO_UNIT, spdif_pio_sm) && already_claimed) {
        // Another setup call, because this does happen twice in certain circumstances
        // There's nothing to set up if it's already been done
        // However, always set the divisor because this is called after final clock shift
        pio_sm_set_clkdiv(SPDIF_PIO_UNIT, spdif_pio_sm, clkdiv);
        return;
    } else if (pio_sm_is_claimed(SPDIF_PIO_UNIT, spdif_pio_sm)) {
        // Try claiming a different SM
        spdif_pio_sm++;
    }
    if (pio_sm_is_claimed(SPDIF_PIO_UNIT, spdif_pio_sm)) {
        // Can't find a free SM, fail
        logmsg("Unable to claim PIO SM for Audio Output");
        audio_setup_failed = true;
        return;
    } else {
        pio_sm_claim(SPDIF_PIO_UNIT, spdif_pio_sm);
        int prog_offset = pio_add_program(SPDIF_PIO_UNIT, &shift_program);
        shift_program_init(SPDIF_PIO_UNIT, spdif_pio_sm, prog_offset, SPDIF_OUTPUT_PIN);
        // Set clock divider
        pio_sm_set_clkdiv(SPDIF_PIO_UNIT, spdif_pio_sm, clkdiv);
        already_claimed = true;
    }

    if (SPDIF_OUTPUT_PIN != GPIO_EXP_SPARE) {
        gpio_put(GPIO_EXP_SPARE, true);
        gpio_set_dir(GPIO_EXP_SPARE, false);
        gpio_set_pulls(GPIO_EXP_SPARE, true, false);
        gpio_set_function(GPIO_EXP_SPARE, GPIO_FUNC_SIO);
    }

    dma_channel_claim(SOUND_DMA_CHA);
	dma_channel_claim(SOUND_DMA_CHB);

#ifdef AUDIO_DMA_IRQ_NUM
    irq_set_exclusive_handler(AUDIO_DMA_IRQ_NUM, audio_dma_irq);
    //irq_add_shared_handler(AUDIO_DMA_IRQ_NUM, audio_dma_irq, 0xFF);
    irq_set_enabled(AUDIO_DMA_IRQ_NUM, true);
    irq_clear(AUDIO_DMA_IRQ_NUM);
# if AUDIO_DMA_IRQ_NUM != DMA_IRQ_0
#  error Legacy code does not currently support irq != 0
# endif
#endif
}

void audio_poll() {
    if (!audio_is_active() || !g_scsi_settings.getSystem()->enableCDAudio || audio_setup_failed) return;
    if (audio_paused) return;
    if (fleft == 0 && sbufst_a == STALE && sbufst_b == STALE) {
        // out of data and ready to stop
        audio_stop(audio_owner);
        return;
    } else if (fleft == 0) {
        // out of data to read but still working on remainder
        return;
    } else if (!audio_file->isOpen()) {
        // closed elsewhere, maybe disk ejected?
        dbgmsg("------ Playback stop due to closed file");
        audio_stop(audio_owner);
        return;
    }

    // are new audio samples needed from the memory card?
    uint8_t* audiobuf;
    if (sbufst_a == STALE) {
        sbufst_a = FILLING;
        audiobuf = sample_buf_a;
    } else if (sbufst_b == STALE) {
        sbufst_b = FILLING;
        audiobuf = sample_buf_b;
    } else {
        // no data needed this time
        return;
    }

    platform_set_sd_callback(NULL, NULL);
    uint16_t toRead = AUDIO_BUFFER_SIZE;
    if (fleft < toRead) toRead = fleft;
    if (audio_file->position() != fpos) {
        // should be uncommon due to SCSI command restrictions on devices
        // playing audio; if this is showing up in logs a different approach
        // will be needed to avoid seek performance issues on FAT32 vols
        dbgmsg("------ Audio seek required on ", audio_owner);
        if (!audio_file->seek(fpos)) {
            logmsg("Audio error, unable to seek to ", fpos, ", ID:", audio_owner);
        }
    }
    if (audio_file->read(audiobuf, toRead) != toRead) {
        logmsg("Audio sample data underrun");
    }
    fpos += toRead;
    fleft -= toRead;

    if (sbufst_a == FILLING) {
        sbufst_a = READY;
    } else if (sbufst_b == FILLING) {
        sbufst_b = READY;
    }
}

bool audio_play(uint8_t owner, image_config_t* img, uint64_t start, uint64_t end, bool swap) {
    // stop any existing playback first
    if (audio_is_active()) audio_stop(audio_owner);
    // Don't try to play audio if setup failed
    if (audio_setup_failed) {
        return true;
    }

    // dbgmsg("Request to play ('", file, "':", start, ":", end, ")");

    // verify audio file is present and inputs are (somewhat) sane
    if (owner == 0xFF) {
        logmsg("Illegal audio owner");
        return false;
    }
    if (start >= end) {
        logmsg("Invalid range for audio (", start, ":", end, ")");
        return false;
    }
    platform_set_sd_callback(NULL, NULL);
    audio_file = &img->file;
    if (!audio_file->isOpen()) {
        logmsg("File not open for audio playback, ", owner);
        return false;
    }
    uint64_t len = audio_file->size();
    if (start > len) {
        logmsg("File playback request start (", start, ":", len, ") outside file bounds");
        return false;
    }
    // truncate playback end to end of file
    // we will not consider this to be an error at the moment
    if (end > len) {
        dbgmsg("------ Truncate audio play request end ", end, " to file size ", len);
        end = len;
    }
    fleft = end - start;
    if (fleft <= 2 * AUDIO_BUFFER_SIZE) {
        logmsg("File playback request (", start, ":", end, ") too short");
        return false;
    }

    // read in initial sample buffers
    if (!audio_file->seek(start)) {
        logmsg("Sample file failed start seek to ", start);
        return false;
    }
    if (audio_file->read(sample_buf_a, AUDIO_BUFFER_SIZE) != AUDIO_BUFFER_SIZE) {
        logmsg("File playback start returned fewer bytes than allowed");
        return false;
    }
    if (audio_file->read(sample_buf_b, AUDIO_BUFFER_SIZE) != AUDIO_BUFFER_SIZE) {
        logmsg("File playback start returned fewer bytes than allowed");
        return false;
    }

    // prepare initial tracking state
    fpos = audio_file->position();
    fleft -= AUDIO_BUFFER_SIZE * 2;
    sbufsel = A;
    sbufpos = 0;
    sbufswap = swap;
    sbufst_a = READY;
    sbufst_b = READY;
    audio_owner = owner & 7;
    audio_last_status[audio_owner] = ASC_PLAYING;
    audio_paused = false;

    // prepare the wire buffers
    for (uint16_t i = 0; i < WIRE_BUFFER_SIZE; i++) {
        wire_buf_a[i] = 0;
        wire_buf_b[i] = 0;
    }
    sfcnt = 0;
    invert = 0;

    // setup the two DMA units to hand-off to each other
    // to maintain a stable bitstream these need to run without interruption
	snd_dma_a_cfg = dma_channel_get_default_config(SOUND_DMA_CHA);
	channel_config_set_transfer_data_size(&snd_dma_a_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&snd_dma_a_cfg, pio_get_dreq(SPDIF_PIO_UNIT, spdif_pio_sm, true));
	channel_config_set_read_increment(&snd_dma_a_cfg, true);
	channel_config_set_chain_to(&snd_dma_a_cfg, SOUND_DMA_CHB);
    // version of pico-sdk lacks channel_config_set_high_priority()
    snd_dma_a_cfg.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;
	dma_channel_configure(SOUND_DMA_CHA, &snd_dma_a_cfg, &SPDIF_PIO_UNIT->txf[spdif_pio_sm],
			&wire_buf_a, WIRE_BUFFER_SIZE, false);
    dma_channel_set_irq0_enabled(SOUND_DMA_CHA, true);

	snd_dma_b_cfg = dma_channel_get_default_config(SOUND_DMA_CHB);
	channel_config_set_transfer_data_size(&snd_dma_b_cfg, DMA_SIZE_16);
	channel_config_set_dreq(&snd_dma_b_cfg, pio_get_dreq(SPDIF_PIO_UNIT, spdif_pio_sm, true));
	channel_config_set_read_increment(&snd_dma_b_cfg, true);
	channel_config_set_chain_to(&snd_dma_b_cfg, SOUND_DMA_CHA);
    snd_dma_b_cfg.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;
	dma_channel_configure(SOUND_DMA_CHB, &snd_dma_b_cfg, &SPDIF_PIO_UNIT->txf[spdif_pio_sm],
			&wire_buf_b, WIRE_BUFFER_SIZE, false);
    dma_channel_set_irq0_enabled(SOUND_DMA_CHB, true);

    // ready to go
    dma_channel_start(SOUND_DMA_CHA);
    return true;
}

bool audio_set_paused(uint8_t id, bool paused) {
    if (audio_owner != (id & 7)) return false;
    else if (audio_paused && paused) return false;
    else if (!audio_paused && !paused) return false;

    audio_paused = paused;
    if (paused) {
        audio_last_status[audio_owner] = ASC_PAUSED;
    } else {
        audio_last_status[audio_owner] = ASC_PLAYING;
    }
    return true;
}

void audio_stop(uint8_t id) {
    // If setup failed, playback also failed, no need to stop
    if (audio_setup_failed) {
        return;
    }
    if (audio_owner != (id & 7)) return;

    // to help mute external hardware, send a bunch of '0' samples prior to
    // halting the datastream; easiest way to do this is invalidating the
    // sample buffers, same as if there was a sample data underrun
    sbufst_a = STALE;
    sbufst_b = STALE;

    // then indicate that the streams should no longer chain to one another
    // and wait for them to shut down naturally
    audio_stopping = true;
    while (dma_channel_is_busy(SOUND_DMA_CHA)) tight_loop_contents();
    while (dma_channel_is_busy(SOUND_DMA_CHB)) tight_loop_contents();
    while (!pio_sm_is_tx_fifo_empty(SPDIF_PIO_UNIT, spdif_pio_sm)) tight_loop_contents();
    audio_stopping = false;

    // idle the subsystem
    audio_last_status[audio_owner] = ASC_COMPLETED;
    audio_paused = false;
    audio_owner = 0xFF;
}

audio_status_code audio_get_status_code(uint8_t id) {
    audio_status_code tmp = audio_last_status[id & 7];
    if (tmp == ASC_COMPLETED || tmp == ASC_ERRORED) {
        audio_last_status[id & 7] = ASC_NO_STATUS;
    }
    return tmp;
}

uint16_t audio_get_volume(uint8_t id) {
    return volumes[id & 7];
}

void audio_set_volume(uint8_t id, uint16_t vol) {
    volumes[id & 7] = vol;
}

uint16_t audio_get_channel(uint8_t id) {
    return channels[id & 7];
}

void audio_set_channel(uint8_t id, uint16_t chn) {
    channels[id & 7] = chn;
}

uint64_t audio_get_file_position()
{
    return fpos;
}

void audio_set_file_position(uint8_t id, uint32_t lba)
{
    fpos = 2352 * (uint64_t)lba;

}
#endif // ENABLE_AUDIO_OUTPUT_SPDIF
