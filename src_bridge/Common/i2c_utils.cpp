/**
 * RP2040 FreeRTOS Template - App #2
 * Generic I2C functions
 *
 * @copyright 2024, Tony Smith (@smittytone)
 * @version   1.5.0
 * @licence   MIT
 *
 */
#include "i2c_utils.h"


namespace I2C {

/**
 * @brief Set up the I2C block.
 *
 * Takes values from #defines set in `i2c_utils.h`
 */
void setup(void) {

    i2c_init(I2C_PORT, I2C_FREQUENCY);
    gpio_set_function(SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_GPIO);
    gpio_pull_up(SCL_GPIO);
}

/**
 * @brief Convenience function to write a single byte to the bus.
 *
 * @param address: The I2C address of the device to write to.
 * @param byte:    The byte to send.
 */
void write_byte(uint8_t address, uint8_t byte) {

    i2c_write_blocking(I2C_PORT, address, &byte, 1, false);
}

/**
 * @brief Convenience function to write bytes to the bus.
 *
 * @param address: The I2C address of the device to write to.
 * @param data:    Pointer to the bytes to send.
 * @param count:   The number of bytes to send.
 */
void write_block(uint8_t address, const uint8_t* data, uint8_t count) {

    i2c_write_blocking(I2C_PORT, address, data, count, false);
}

/**
 * @brief Convenience function to read bytes from the bus.
 *
 * @param address: The I2C address of the device to read from.
 * @param data:    Pointer to byte storage.
 * @param count:   The number of bytes to read.
 */
void read_block(uint8_t address, uint8_t *data, uint8_t count) {

    i2c_read_blocking(I2C_PORT, address, data, count, false);
}


}   // namespace I2C
