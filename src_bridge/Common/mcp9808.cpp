/**
 * RP2040 FreeRTOS Template - App #3
 * MCP9808 I2C temperature sensor driver
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#include "mcp9808.h"

using std::string;


/**
 * @brief Constructor: instantiate a new MCP9808 object.
 *
 * @param address: The I2C address of the device to write to.
 */
MCP9808::MCP9808(uint8_t address) {

    if (address == 0x00 || address > 0xFF) address = MCP9808_I2CADDR_DEFAULT;
    i2c_addr = address;

    // Set defaults
    limit_lower = DEFAULT_TEMP_LOWER_LIMIT_C;
    limit_upper = DEFAULT_TEMP_UPPER_LIMIT_C;
    limit_critical = DEFAULT_TEMP_CRIT_LIMIT_C;
}


/**
 * @brief Check the device is connected and operational.
 *
 * @retval `true` if we can read values and they are right, otherwise `false`.
 */
bool MCP9808::begin(void) {

    // Set up alerts threshold temperatures
    // NOTE You MUST set all three thresholds
    //      for the alert to operate.
    set_lower_limit(limit_lower);
    set_upper_limit(limit_upper);
    set_critical_limit(limit_critical);

    // Clear and enable the alert pin
    clear_alert(true);

    // Prep data storage buffers
    uint8_t mid_data[2] = {0,0};
    uint8_t did_data[2] = {0,0};

    // Read bytes from the sensor: MID...
    I2C::write_byte(i2c_addr, MCP9808_REG_MANUF_ID);
    I2C::read_block(i2c_addr, mid_data, 2);

    // ...DID
    I2C::write_byte(i2c_addr, MCP9808_REG_DEVICE_ID);
    I2C::read_block(i2c_addr, did_data, 2);

    // Bytes to integers
    const auto mid_value = (uint16_t)((mid_data[0] << 8) | mid_data[1]);
    const auto did_value = (uint16_t)((did_data[0] << 8) | did_data[1]);

    // Returns True if the device is initialised, False otherwise.
    return (mid_value == 0x0054 && did_value == 0x0400);
}


/**
 * @brief Read the temperature.
 *
 * @retval The temperature in Celsius.
 */
double MCP9808::read_temp(void) const {

    // Read sensor and return its value in degrees celsius.
    uint8_t temp_data[2] = {0};
    I2C::write_byte(i2c_addr, MCP9808_REG_AMBIENT_TEMP);
    I2C::read_block(i2c_addr, temp_data, 2);

    // Scale and convert to signed value.
    return get_temp(temp_data);
}


/**
 * @brief Clear the sensor's alert flag, CONFIG bit 5.
 *        Optionally, enable the alert first.
 *
 * @param do_enable: Set to `true` to enable the alert.
 */
void MCP9808::clear_alert(bool do_enable) const {

    // Read the current reg value
    uint8_t config_data[3] = {0};
    I2C::write_byte(i2c_addr, MCP9808_REG_CONFIG);
    I2C::read_block(i2c_addr, &config_data[1], 2);

    // Set LSB bit 5 to clear the interrupt, and write it back
    config_data[0] = MCP9808_REG_CONFIG;
    config_data[2] = 0x21;

    if (do_enable) {
        config_data[2] |= MCP9808_CONFIG_ENABLE_ALRT;
    }

    // Write config data back with changes
#ifdef DEBUG
    printf("[DEBUG] MCP9809 alert config write: %02x %02x %02x\n",  config_data[0],  config_data[1],  config_data[2]);
#endif
    I2C::write_block(i2c_addr, config_data, 3);

    // Read it back to apply?
    uint8_t check_data[2] = {0};
    I2C::write_byte(i2c_addr, MCP9808_REG_CONFIG);
    I2C::read_block(i2c_addr, check_data, 2);
#ifdef DEBUG
    printf("[DEBUG] MCP9809 alert config read:  -- %02x %02x\n",  check_data[0],  check_data[1]);
#endif
}


/**
 * @brief Set the sensor upper threshold temperature.
 *
 * @param upper_temp: The target temperature.
 */
void MCP9808::set_upper_limit(uint16_t upper_temp) {

    limit_upper = upper_temp;
    set_temp_limit(MCP9808_REG_UPPER_TEMP, upper_temp);
}


/**
 * @brief Set the sensor critical threshold temperature.
 *
 * @param critical_temp: The target temperature.
 */
void MCP9808::set_critical_limit(uint16_t critical_temp) {

    limit_critical = critical_temp;
    set_temp_limit(MCP9808_REG_CRIT_TEMP, critical_temp);
}


/**
 * @brief Set the sensor lower threshold temperature.
 *
 * @param lower_temp: The target temperature.
 */
void MCP9808::set_lower_limit(uint16_t lower_temp) {

    limit_lower = lower_temp;
    set_temp_limit(MCP9808_REG_LOWER_TEMP, lower_temp);
}


/**
 * @brief Set a sensor threshold temperature.
 *
 * @param temp_register: The target register:
 *                       MCP9808_REG_LOWER_TEMP
 *                       MCP9808_REG_UPPER_TEMP
 *                       MCP9808_REG_CRIT_TEMP.
 * @param temp:          The temperature (as an integer)
 */
void MCP9808::set_temp_limit(uint8_t temp_register, uint16_t temp) const {

    temp &= 127;
    temp = (uint8_t)(temp << 4);
    uint8_t data[3] = {temp_register};
    data[1] = (temp & 0xFF00) >> 8;
    data[2] = temp & 0xFF;
    I2C::write_block(i2c_addr, data, 3);

    // Read and check upper temp
#ifdef DEBUG
    string reg_name = "Lower Limit";
    if (temp_register == MCP9808_REG_CRIT_TEMP) {
        reg_name = "Critical Limit";
    } else if (temp_register == MCP9808_REG_UPPER_TEMP) {
        reg_name = "Upper Limit";
    }

    I2C::write_byte(i2c_addr, temp_register);
    I2C::read_block(i2c_addr, data, 2);
    double temp_cel = get_temp(data);
    printf("[DEBUG] %s: %.01f\n", reg_name.c_str(), temp_cel);
#endif
}


/**
 * @brief Calculate the temperature.
 *
 * @retval The temperature in Celsius.
 */
double MCP9808::get_temp(const uint8_t* data) const {

    const uint32_t temp_raw = (data[0] << 8) | data[1];
    double temp_cel = (temp_raw & 0x0FFF) / 16.0;
    if (temp_raw & 0x1000) temp_cel = 256.0 - temp_cel;
    return temp_cel;
}
