#include "unity.h"
#include "usb_descriptor.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_device_descriptor_default_constructor(void) {
    USB::DeviceDescriptor device;
    TEST_ASSERT_EQUAL(sizeof(tusb_desc_device_t), static_cast<tusb_desc_device_t>(device).bLength);
    TEST_ASSERT_EQUAL(TUSB_DESC_DEVICE, static_cast<tusb_desc_device_t>(device).bDescriptorType);
    TEST_ASSERT_EQUAL(0, device.getNumConfigurations());
}

void test_device_descriptor_set_device_info(void) {
    USB::DeviceDescriptor device;
    
    device.setUSBVersion(0x0200);      // USB 2.0
    device.setDeviceClass(0xFF);       // Vendor specific
    device.setVendorID(0x0483);
    device.setProductID(0x5740);
    device.setDeviceVersion(0x0100);   // v1.0.0
    
    TEST_ASSERT_EQUAL(0x0200, device.getUSBVersion());
    TEST_ASSERT_EQUAL(0xFF, device.getDeviceClass());
    TEST_ASSERT_EQUAL(0x0483, device.getVendorID());
    TEST_ASSERT_EQUAL(0x5740, device.getProductID());
    TEST_ASSERT_EQUAL(0x0100, device.getDeviceVersion());
}

void test_device_descriptor_add_configuration(void) {
    USB::DeviceDescriptor device;
    USB::ConfigurationDescriptor config;
    
    config.setConfigurationValue(1);
    bool result = device.addConfiguration(config);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, device.getNumConfigurations());
    
    // Try adding duplicate configuration
    result = device.addConfiguration(config);
    TEST_ASSERT_FALSE(result);
}

void test_device_descriptor_generate_block(void) {
    USB::DeviceDescriptor device;
    device.setUSBVersion(0x0200);
    device.setVendorID(0x0483);
    device.setProductID(0x5740);
    
    std::vector<uint8_t> block = device.generateDescriptorBlock();
    
    TEST_ASSERT_EQUAL(sizeof(tusb_desc_device_t), block.size());
    TEST_ASSERT_EQUAL(TUSB_DESC_DEVICE, block[1]);  // bDescriptorType
    TEST_ASSERT_EQUAL(0x00, block[2]);  // bcdUSB LSB
    TEST_ASSERT_EQUAL(0x02, block[3]);  // bcdUSB MSB
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_device_descriptor_default_constructor);
    RUN_TEST(test_device_descriptor_set_device_info);
    RUN_TEST(test_device_descriptor_add_configuration);
    RUN_TEST(test_device_descriptor_generate_block);
    return UNITY_END();
}