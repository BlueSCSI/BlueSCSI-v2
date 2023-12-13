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

/**
 * Configuration of the ZuluSCSI set by hardware rotary and standard DIP switches
 * Settings include SCSI ID, device type, and if hardware config is active 
*/

#pragma once
#ifdef ZULUSCSI_HARDWARE_CONFIG
#include <scsi2sd.h>
#include <ZuluSCSI_settings.h>

// C wrappers
#ifdef __cplusplus
extern "C"
{
#endif
    S2S_CFG_TYPE hw_config_selected_device();
    bool hw_config_is_active();
    void hw_config_init_gpios();
    void hw_config_init_state(bool is_active);
    
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class HardwareConfig
{
public:
    // Initialize GPIOs
    void init_gpios();

    // Initialize device state settings
    void init_state(bool is_active);

    // get the device type
    // @returns the device type
    const S2S_CFG_TYPE& device_type() const {return m_device_type;}
    const uint8_t& scsi_id()          const {return m_scsi_id;}
    const bool& is_active()           const {return m_is_active;}
    const int blocksize()             const {return m_blocksize;}
    const scsi_device_preset_t device_preset() const {return m_device_preset;}

protected:
    S2S_CFG_TYPE m_device_type;
    uint8_t m_scsi_id;
    bool m_is_active;
    int m_blocksize;
    scsi_device_preset_t m_device_preset;
};


// global hardware configuration
extern HardwareConfig g_hw_config;

#endif // __cplusplus
#endif // ZULUSCSI_HARDWARE_CONFIG