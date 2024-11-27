#include "unity.h"
#include "usb_descriptor.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_configuration_descriptor_default_constructor(void) {
    USB::ConfigurationDescriptor config;
    TEST_ASSERT_EQUAL(sizeof(tusb_desc_configuration_t), static_cast<tusb_desc_configuration_t>(config).bLength);
    TEST_ASSERT_EQUAL(TUSB_DESC_CONFIGURATION, static_cast<tusb_desc_configuration_t>(config).bDescriptorType);
    TEST_ASSERT_EQUAL(1, config.getConfigurationValue());
    TEST_ASSERT_EQUAL(0, config.getNumInterfaces());
}

void test_configuration_descriptor_add_interface(void) {
    USB::ConfigurationDescriptor config;
    USB::InterfaceDescriptor iface;
    
    iface.setInterfaceNumber(0);
    bool result = config.addInterface(iface);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, config.getNumInterfaces());
    
    // Try adding duplicate interface number
    result = config.addInterface(iface);
    TEST_ASSERT_FALSE(result);
}

void test_configuration_descriptor_total_length_calculation(void) {
    USB::ConfigurationDescriptor config;
    USB::InterfaceDescriptor iface;
    USB::EndpointDescriptor ep_in, ep_out;
    
    iface.setInterfaceNumber(0);
    ep_in.setEndpointAddress(0x81);
    ep_out.setEndpointAddress(0x01);
    
    iface.addEndpoint(ep_in);
    iface.addEndpoint(ep_out);
    config.addInterface(iface);
    
    uint16_t expected_length = sizeof(tusb_desc_configuration_t) +
                              sizeof(tusb_desc_interface_t) +
                              2 * sizeof(tusb_desc_endpoint_t);
    
    TEST_ASSERT_EQUAL(expected_length, config.getTotalLength());
}

void test_configuration_descriptor_generate_block(void) {
    USB::ConfigurationDescriptor config;
    USB::InterfaceDescriptor iface;
    USB::EndpointDescriptor ep;
    
    config.setConfigurationValue(1);
    iface.setInterfaceNumber(0);
    ep.setEndpointAddress(0x81);
    
    iface.addEndpoint(ep);
    config.addInterface(iface);
    
    std::vector<uint8_t> block = config.generateDescriptorBlock();
    
    uint16_t expected_length = sizeof(tusb_desc_configuration_t) +
                              sizeof(tusb_desc_interface_t) +
                              sizeof(tusb_desc_endpoint_t);
    
    TEST_ASSERT_EQUAL(expected_length, block.size());
    TEST_ASSERT_EQUAL(TUSB_DESC_CONFIGURATION, block[1]);  // bDescriptorType
    TEST_ASSERT_EQUAL(1, block[5]);  // bConfigurationValue
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_configuration_descriptor_default_constructor);
    RUN_TEST(test_configuration_descriptor_add_interface);
    RUN_TEST(test_configuration_descriptor_total_length_calculation);
    RUN_TEST(test_configuration_descriptor_generate_block);
    return UNITY_END();
}