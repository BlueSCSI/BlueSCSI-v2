/** 
 * Copyright (C) 2023 saybur
 * ZuluSCSI™ - Copyright (c) 2023 Rabbit Hole Computing™
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
#ifdef ENABLE_AUDIO_OUTPUT_I2S
#include "audio_i2s.h"
#include "ZuluSCSI_platform.h"
#include "ZuluSCSI_audio.h"
#include "ZuluSCSI_v1_1_gpio.h"
#include "ZuluSCSI_log.h"

extern "C" 
{
    #include "gd32f20x_rcu.h" 
    #include "gd32f20x_dma.h"
    #include "gd32f20x_misc.h"
}

bool g_audio_enabled = false;
bool g_audio_stopped = true;

// some chonky buffers to store audio samples
static uint8_t sample_circ_buf[AUDIO_BUFFER_SIZE] __attribute__((aligned(4)));

// tracking for the state for the circular buffer, A first half and B second half
enum bufstate { STALE, FILLING, READY };
static volatile bufstate sbufst_a = STALE;
static volatile bufstate sbufst_b = STALE;
static uint8_t sbufswap = 0;

// tracking for audio playback
static uint8_t audio_owner; // SCSI ID or 0xFF when idle
static volatile bool audio_paused = false;
static ImageBackingStore* audio_file;
static volatile uint64_t fpos;
static volatile uint32_t fleft;
extern bool g_audio_stopped;


// historical playback status information
static audio_status_code audio_last_status[8] = {ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS,
                                                 ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS, ASC_NO_STATUS};

// volume information for targets
static volatile uint16_t volumes[8] = {
    DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL,
    DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL, DEFAULT_VOLUME_LEVEL
};
static volatile uint16_t channels[8] = {
    AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK,
    AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK, AUDIO_CHANNEL_ENABLE_MASK
};

bool audio_is_active() {
    return audio_owner != 0xFF;
}


bool audio_is_playing(uint8_t id) {
    return audio_owner == (id & 7);
}



static void audio_start_dma()
{  
        dma_channel_disable(ODE_DMA, ODE_DMA_CH);
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_HTF);
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_FTF);
        dma_interrupt_enable(ODE_DMA, ODE_DMA_CH, DMA_INT_HTF | DMA_INT_FTF);
        dma_transfer_number_config(ODE_DMA, ODE_DMA_CH, AUDIO_BUFFER_SIZE / 2); // convert to 16bit transfer count
        spi_enable(ODE_I2S_SPI);
        dma_channel_enable(ODE_DMA, ODE_DMA_CH);

}
extern "C"
{
    void ODE_IRQHandler() 
    {
        if ( SET == dma_interrupt_flag_get(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_HTF))
        {
            dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_HTF);
            sbufst_a = STALE;
        }
        if (SET == dma_interrupt_flag_get(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_FTF))
        {
            dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_FTF);
            sbufst_b = STALE;
        } 
    }
}

void audio_setup() 
{
    // Setup clocks  
    rcu_periph_clock_enable(ODE_RCU_I2S_SPI);
    rcu_periph_clock_enable(ODE_RCU_DMA);

    // Install NVIC
    nvic_irq_enable(ODE_DMA_IRQn, 3, 0);

    // DMA setup
    dma_parameter_struct dma_init_struct;
    dma_struct_para_init(&dma_init_struct);

    dma_deinit(ODE_DMA, ODE_DMA_CH);
    dma_init_struct.periph_addr  = (uint32_t)&SPI_DATA(ODE_I2S_SPI);
    dma_init_struct.memory_addr  = (uint32_t)sample_circ_buf;
    dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_init_struct.priority     = DMA_PRIORITY_LOW;
    dma_init_struct.number       = AUDIO_BUFFER_SIZE / 2; // 8 bit to 16 bit conversion length
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_init(ODE_DMA, ODE_DMA_CH, &dma_init_struct);
    /* configure DMA mode */
    dma_circulation_enable(ODE_DMA, ODE_DMA_CH);
    dma_memory_to_memory_disable(ODE_DMA, ODE_DMA_CH);

    /* configure I2S1 */
    i2s_disable(ODE_I2S_SPI);
    spi_i2s_deinit(ODE_I2S_SPI);
    i2s_init(ODE_I2S_SPI, I2S_MODE_MASTERTX, I2S_STD_PHILLIPS, I2S_CKPL_LOW);
    i2s_psc_config(ODE_I2S_SPI, I2S_AUDIOSAMPLE_44K, I2S_FRAMEFORMAT_DT16B_CH16B, I2S_MCKOUT_DISABLE);
    spi_dma_enable(ODE_I2S_SPI, SPI_DMA_TRANSMIT);
    i2s_enable(ODE_I2S_SPI);

}

/*
 * Takes in a buffer with interleaved 16bit samples and adjusts their volume
*/
static void audio_adjust(uint8_t owner, int16_t* buffer, size_t length)
{
    uint8_t volume[2]; 
    uint16_t packed_volume = volumes[owner & 7];
    volume[0] = packed_volume >> 8;
    volume[1] = packed_volume & 0xFF;

    // enable or disable based on the channel information for both output
    // ports, where the high byte and mask control the right channel, and
    // the low control the left channel    
    uint16_t chn = channels[owner & 7] & AUDIO_CHANNEL_ENABLE_MASK;
    if (!(chn >> 8))
    {
        volume[0] = 0;
    } 
    if (!(chn & 0xFF))
    {
        volume[1] = 0;
    }

    for (int i = 0; i < length; i++)
    {
        // linear volume
        buffer[i] = (int16_t)(( ((int32_t)buffer[i]) * volume[i & 0x1]) >> 8); 
    }

}

void audio_poll() 
{
    if(!g_audio_enabled)
        return;

    if ((!g_audio_stopped) && 1 == (0x1 & platform_get_buttons()))
    {
        audio_stop(audio_owner);
    }
    if (!audio_is_active()) return;
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

    if ( STALE  == sbufst_a || STALE == sbufst_b )
    {
        uint8_t* audiobuf;
        if (sbufst_a == STALE) {
            sbufst_a = FILLING;
            audiobuf = sample_circ_buf;
        } 
        
        if (sbufst_b == STALE) {
            sbufst_b = FILLING;
            audiobuf = sample_circ_buf + AUDIO_BUFFER_HALF_SIZE;
        }

        platform_set_sd_callback(NULL, NULL);
 
        uint16_t toRead = AUDIO_BUFFER_HALF_SIZE;
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
        ssize_t read_length = audio_file->read(audiobuf, AUDIO_BUFFER_HALF_SIZE);
        
        if ( read_length < AUDIO_BUFFER_HALF_SIZE )
        {
            if ( read_length < 0)
            {
                logmsg("Playback file half buffer size read error: ", read_length);    
                return;
            }
            else
            {
                // pad buffer with zeros
                memset(audiobuf + read_length, 0, AUDIO_BUFFER_HALF_SIZE - read_length);
            }
        }
        audio_adjust(audio_owner, (int16_t*) audiobuf, AUDIO_BUFFER_HALF_SIZE / 2);
    
        fpos += toRead;
        fleft -= toRead;

        if (sbufst_a == FILLING) {
            sbufst_a = READY;
        } else if (sbufst_b == FILLING) {
            sbufst_b = READY;
        }
    }
}

bool audio_play(uint8_t owner, image_config_t* img, uint64_t start, uint64_t end, bool swap)
{
    if (audio_is_active()) audio_stop(audio_owner);


    // verify audio file is present and inputs are (somewhat) sane
    if (owner == 0xFF) 
    {
        logmsg("Illegal audio owner");
        return false;
    }
    if (start >= end) 
    {
        logmsg("Invalid range for audio (", start, ":", end, ")");
        return false;
    }

    audio_file = &img->file;
    if (!audio_file->isOpen()) {
        logmsg("File not open for audio playback, ", owner);
        return false;
    }
    uint64_t len = audio_file->size();
    if (start > len) 
    {
        logmsg("File playback request start (", start, ":", len, ") outside file bounds");
        return false;
    }
    // truncate playback end to end of file
    // we will not consider this to be an error at the moment
    if (end > len) 
    {
        dbgmsg("------ Truncate audio play request end ", end, " to file size ", len);
        end = len;
    }
    fleft = end - start;
    if (fleft <= AUDIO_BUFFER_SIZE)
    {
        logmsg("File playback request (", start, ":", end, ") too short");
        return false;
    }

    // read in initial sample buffers
    if (!audio_file->seek(start))
    {
        logmsg("Sample file failed start seek to ", start);
        return false;
    }
    ssize_t read_length = audio_file->read(sample_circ_buf, AUDIO_BUFFER_SIZE);
    if ( read_length < AUDIO_BUFFER_SIZE)
    {
        if ( read_length < 0)
        {
            logmsg("Playback file read error: ", read_length);    
            return false;
        }
        else
        {
            // pad buffer with zeros
            memset(sample_circ_buf + read_length, 0, AUDIO_BUFFER_SIZE - read_length);
        }

    }
    audio_adjust(owner, (int16_t*)sample_circ_buf, AUDIO_BUFFER_SIZE / 2);
 
    // prepare initial tracking state
    fpos = audio_file->position();
    fleft -= AUDIO_BUFFER_SIZE;
    sbufswap = swap;
    sbufst_a = READY;
    sbufst_b = READY;
    audio_owner = owner & 7;
    audio_last_status[audio_owner] = ASC_PLAYING;
    audio_paused = false;
    g_audio_stopped = false;
    audio_start_dma();

    return true;
}

bool audio_set_paused(uint8_t id, bool paused)
{
    if (audio_owner != (id & 7)) return false;
    else if (audio_paused && paused) return false;
    else if (!audio_paused && !paused) return false;



    if (paused) 
    {
        // Turn off interrupts to stop flagging new audio samples to load
        dma_interrupt_disable(ODE_DMA, ODE_DMA_CH, DMA_INT_FTF | DMA_INT_HTF);
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_HTF);
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_FTF);
        // Set status for both left and right channel to stop audio polling
        // from loading new samples
        sbufst_a = READY;
        sbufst_b = READY;
        // Let the DMA continue to run but set audio out to 0s
        memset(sample_circ_buf, 0, AUDIO_BUFFER_SIZE);
        audio_last_status[audio_owner] = ASC_PAUSED;
    } 
    else
    {
        // Enable audio polling and DMA interrupts to load and play new samples
        sbufst_a = STALE;
        sbufst_b = STALE;
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_HTF);
        dma_interrupt_flag_clear(ODE_DMA, ODE_DMA_CH, DMA_INT_FLAG_FTF);
        dma_interrupt_enable(ODE_DMA, ODE_DMA_CH, DMA_INT_FTF | DMA_INT_HTF);
        audio_last_status[audio_owner] = ASC_PLAYING;
    }
    return true;
}

void audio_stop(uint8_t id)
{
    if (audio_owner != (id & 7)) return;

    spi_disable(ODE_I2S_SPI);
    dma_channel_disable(ODE_DMA, ODE_DMA_CH);

    // idle the subsystem
    audio_last_status[audio_owner] = ASC_COMPLETED;
    audio_paused = false;
    g_audio_stopped = true;
    audio_owner = 0xFF;
}

audio_status_code audio_get_status_code(uint8_t id)
{
    audio_status_code tmp = audio_last_status[id & 7];
    if (tmp == ASC_COMPLETED || tmp == ASC_ERRORED) {
        audio_last_status[id & 7] = ASC_NO_STATUS;
    }
    return tmp;
}

uint16_t audio_get_volume(uint8_t id)
{
    return volumes[id & 7];
}

void audio_set_volume(uint8_t id, uint16_t vol)
{
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

#endif // ENABLE_AUDIO_OUTPUT_I2S