#include "unity.h"
#include "usb_descriptor.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_interface_descriptor_default_constructor(void) {
    USB::InterfaceDescriptor iface;
    TEST_ASSERT_EQUAL(sizeof(tusb_desc_interface_t), static_cast<tusb_desc_interface_t>(iface).bLength);
    TEST_ASSERT_EQUAL(TUSB_DESC_INTERFACE, static_cast<tusb_desc_interface_t>(iface).bDescriptorType);
    TEST_ASSERT_EQUAL(0, iface.getNumEndpoints());
}

void test_interface_descriptor_add_endpoint(void) {
    USB::InterfaceDescriptor iface;
    USB::EndpointDescriptor ep;
    
    ep.setEndpointAddress(0x81);
    bool result = iface.addEndpoint(ep);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, iface.getNumEndpoints());
    TEST_ASSERT_EQUAL(1, iface.getEndpoints().size());
    TEST_ASSERT_EQUAL(0x81, iface.getEndpoints()[0].getEndpointAddress());
}

void test_interface_descriptor_class_settings(void) {
    USB::InterfaceDescriptor iface;
    
    iface.setInterfaceClass(TUSB_CLASS_MSC);
    iface.setInterfaceSubClass(0x06);  // SCSI transparent command set
    iface.setInterfaceProtocol(0x50);  // Bulk-Only Transport
    
    TEST_ASSERT_EQUAL(TUSB_CLASS_MSC, iface.getInterfaceClass());
    TEST_ASSERT_EQUAL(0x06, iface.getInterfaceSubClass());
    TEST_ASSERT_EQUAL(0x50, iface.getInterfaceProtocol());
}

void test_interface_descriptor_multiple_endpoints(void) {
    USB::InterfaceDescriptor iface;
    USB::EndpointDescriptor ep_in, ep_out;
    
    ep_in.setEndpointAddress(0x81);
    ep_out.setEndpointAddress(0x01);
    
    iface.addEndpoint(ep_in);
    iface.addEndpoint(ep_out);
    
    TEST_ASSERT_EQUAL(2, iface.getNumEndpoints());
    TEST_ASSERT_EQUAL(2, iface.getEndpoints().size());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_interface_descriptor_default_constructor);
    RUN_TEST(test_interface_descriptor_add_endpoint);
    RUN_TEST(test_interface_descriptor_class_settings);
    RUN_TEST(test_interface_descriptor_multiple_endpoints);
    return UNITY_END();
}