/** 
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
#include "ZuluSCSI_platform.h"

#ifdef ZULUSCSI_HARDWARE_CONFIG
#include "platform_hw_config.h"
#include <ZuluSCSI_config.h>
#include <ZuluSCSI_log.h>
#include <ZuluSCSI_settings.h>

HardwareConfig g_hw_config;

bool hw_config_is_active()
{
    return g_hw_config.is_active();
}

void hw_config_init_gpios()
{
    g_hw_config.init_gpios();
}

void hw_config_init_state(bool is_active)
{
    g_hw_config.init_state(is_active);
}

void HardwareConfig::init_gpios()
{
    // SCSI ID dip switch
    gpio_init(DIPSW_SCSI_ID_BIT_PORT, GPIO_MODE_IPD, 0, DIPSW_SCSI_ID_BIT_PINS);
    
    // Device select BCD rotary dip switch
    gpio_init(DIPROT_DEVICE_SEL_BIT_PORT, GPIO_MODE_IPD, 0, DIPROT_DEVICE_SEL_BIT_PINS);

    LED_EJECT_OFF();
    gpio_init(LED_EJECT_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_EJECT_PIN);
}

void HardwareConfig::init_state(bool is_active)
{
    m_is_active = is_active;
    m_scsi_id = (gpio_input_port_get(DIPSW_SCSI_ID_BIT_PORT) & DIPSW_SCSI_ID_BIT_PINS) >> DIPSW_SCSI_ID_BIT_SHIFT;
    m_device_preset = DEV_PRESET_NONE;
    logmsg("SCSI ID set via DIP switch to ", m_scsi_id);
    scsi_device_settings_t &cfg_dev = *g_scsi_settings.getDevice(m_scsi_id);

    uint8_t rotary_select = (gpio_input_port_get(DIPROT_DEVICE_SEL_BIT_PORT) & DIPROT_DEVICE_SEL_BIT_PINS) >> DIPROT_DEVICE_SEL_BIT_SHIFT;
    switch (rotary_select)
    {
    case 0:
         cfg_dev.deviceType = S2S_CFG_FIXED;
    break;
    case 1:
        cfg_dev.deviceType = S2S_CFG_OPTICAL;
    break;
    case 2:
        cfg_dev.deviceType = S2S_CFG_FLOPPY_14MB;
    break;
    case 3:
        cfg_dev.deviceType = S2S_CFG_REMOVABLE;
    break;
    case 4:
        cfg_dev.deviceType = S2S_CFG_MO;
    break;
    case 5:
        m_device_preset = DEV_PRESET_ST32430N;
        cfg_dev.deviceType = S2S_CFG_FIXED;
    break;
    case 6:
        cfg_dev.deviceType = S2S_CFG_SEQUENTIAL;
    break;
    default:
        cfg_dev.deviceType = S2S_CFG_FIXED;
    }
    
    if (cfg_dev.deviceType == S2S_CFG_OPTICAL)
    {
        m_blocksize = DEFAULT_BLOCKSIZE_OPTICAL;
    }
    else
    {
        m_blocksize = RAW_FALLBACK_BLOCKSIZE;
    }


}

#endif // ZULUSCSI_HARDWARE_CONFIG