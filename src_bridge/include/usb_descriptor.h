#ifndef USB_DESCRIPTOR_H_
#define USB_DESCRIPTOR_H_


// Example usage (according to Claude.AI):
//      USB::DeviceDescriptor device;
//      device.setUSBVersion(0x0200);
//      device.setDeviceClass(0xFF);
//      
//      USB::ConfigurationDescriptor config;
//      config.setConfigurationValue(1);
//      
//      USB::InterfaceDescriptor interface;
//      interface.setInterfaceNumber(0);
//      
//      USB::EndpointDescriptor endpoint;
//      endpoint.setEndpointAddress(0x81);
//      endpoint.setTransferType(TUSB_XFER_BULK);

#include <vector>
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include "tusb.h"

namespace USB
{

    class EndpointDescriptor
    {
    public:
        EndpointDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_endpoint_t);
            desc_.bDescriptorType = TUSB_DESC_ENDPOINT;
        }

        explicit EndpointDescriptor(const tusb_desc_endpoint_t &desc) : desc_(desc) {}

        // Getters
        uint8_t getEndpointAddress() const { return desc_.bEndpointAddress; }
        uint8_t getAttributes() const
        {
            uint8_t attributes = 0;
            attributes |= (desc_.bmAttributes.xfer << 0);
            attributes |= (desc_.bmAttributes.sync << 2);
            attributes |= (desc_.bmAttributes.usage << 4);
            return attributes;
        }
        uint16_t getMaxPacketSize() const { return desc_.wMaxPacketSize; }
        uint8_t getInterval() const { return desc_.bInterval; }

        // Setters
        void setEndpointAddress(uint8_t address) { desc_.bEndpointAddress = address; }
        void setAttributes(uint8_t attributes)
        {
            desc_.bmAttributes.xfer = (attributes & 0x03);
            desc_.bmAttributes.sync = (attributes & 0x0C) >> 2;
            desc_.bmAttributes.usage = (attributes & 0x30) >> 4;
        }
        void setMaxPacketSize(uint16_t size) { desc_.wMaxPacketSize = size; }
        void setInterval(uint8_t interval) { desc_.bInterval = interval; }

        void setDirection(tusb_dir_t dir)
        {
            if (dir == TUSB_DIR_IN)
            {
                desc_.bEndpointAddress |= TUSB_DIR_IN_MASK;
            }
            else
            {
                desc_.bEndpointAddress &= ~TUSB_DIR_IN_MASK;
            }
        }

        void setTransferType(tusb_xfer_type_t type)
        {
            desc_.bmAttributes.xfer = type;
        }

        operator tusb_desc_endpoint_t() const { return desc_; }

    protected:
        tusb_desc_endpoint_t desc_;
    };

    class InterfaceDescriptor
    {
    public:
        InterfaceDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_interface_t);
            desc_.bDescriptorType = TUSB_DESC_INTERFACE;
            desc_.bNumEndpoints = 0;
        }

        explicit InterfaceDescriptor(const tusb_desc_interface_t &desc) : desc_(desc) {}

        // Getters
        uint8_t getInterfaceNumber() const { return desc_.bInterfaceNumber; }
        uint8_t getAlternateSetting() const { return desc_.bAlternateSetting; }
        uint8_t getNumEndpoints() const { return desc_.bNumEndpoints; }
        uint8_t getInterfaceClass() const { return desc_.bInterfaceClass; }
        uint8_t getInterfaceSubClass() const { return desc_.bInterfaceSubClass; }
        uint8_t getInterfaceProtocol() const { return desc_.bInterfaceProtocol; }
        uint8_t getStringIndex() const { return desc_.iInterface; }

        // Setters
        void setInterfaceNumber(uint8_t num) { desc_.bInterfaceNumber = num; }
        void setAlternateSetting(uint8_t alt) { desc_.bAlternateSetting = alt; }
        void setNumEndpoints(uint8_t num) { desc_.bNumEndpoints = num; }
        void setInterfaceClass(uint8_t cls) { desc_.bInterfaceClass = cls; }
        void setInterfaceSubClass(uint8_t subcls) { desc_.bInterfaceSubClass = subcls; }
        void setInterfaceProtocol(uint8_t protocol) { desc_.bInterfaceProtocol = protocol; }
        void setStringIndex(uint8_t index) { desc_.iInterface = index; }

        bool addEndpoint(const EndpointDescriptor &ep);
        const std::vector<EndpointDescriptor> &getEndpoints() const { return endpoints_; }

        operator tusb_desc_interface_t() const { return desc_; }

    protected:
        tusb_desc_interface_t desc_;
        std::vector<EndpointDescriptor> endpoints_;
    };

    class ConfigurationDescriptor
    {
    public:
        ConfigurationDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_configuration_t);
            desc_.bDescriptorType = TUSB_DESC_CONFIGURATION;
            desc_.bNumInterfaces = 0;
            desc_.bConfigurationValue = 1;
            desc_.bmAttributes = TU_BIT(7) | TUSB_DESC_CONFIG_ATT_SELF_POWERED;
            desc_.bMaxPower = TUSB_DESC_CONFIG_POWER_MA(500);
        }

        explicit ConfigurationDescriptor(const tusb_desc_configuration_t &desc) : desc_(desc) {}

        // Getters
        uint16_t getTotalLength() const { return desc_.wTotalLength; }
        uint8_t getNumInterfaces() const { return desc_.bNumInterfaces; }
        uint8_t getConfigurationValue() const { return desc_.bConfigurationValue; }
        uint8_t getStringIndex() const { return desc_.iConfiguration; }
        uint8_t getAttributes() const { return desc_.bmAttributes; }
        uint8_t getMaxPower() const { return desc_.bMaxPower; }

        // Setters
        void setConfigurationValue(uint8_t value) { desc_.bConfigurationValue = value; }
        void setStringIndex(uint8_t index) { desc_.iConfiguration = index; }
        void setAttributes(uint8_t attributes) { desc_.bmAttributes = attributes; }
        void setMaxPower(uint8_t maxPower) { desc_.bMaxPower = maxPower; }

        bool addInterface(const InterfaceDescriptor *interface);
        // const std::vector<InterfaceDescriptor> &getInterfaces() const { return interfaces_; }
        const InterfaceDescriptor *getInterface(uint8_t interfaceNumber) const;
        std::vector<uint8_t> generateDescriptorBlock() const;

        operator tusb_desc_configuration_t() const { return desc_; }

    protected:
        tusb_desc_configuration_t desc_;
        std::vector<const InterfaceDescriptor*> interfaces_;

        void updateDescriptor();
    };

    class DeviceDescriptor
    {
    public:
        DeviceDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_device_t);
            desc_.bDescriptorType = TUSB_DESC_DEVICE;
            desc_.bNumConfigurations = 0;
        }

        explicit DeviceDescriptor(const tusb_desc_device_t &desc) : desc_(desc) {}

        // Getters
        uint16_t getUSBVersion() const { return desc_.bcdUSB; }
        uint8_t getDeviceClass() const { return desc_.bDeviceClass; }
        uint8_t getDeviceSubClass() const { return desc_.bDeviceSubClass; }
        uint8_t getDeviceProtocol() const { return desc_.bDeviceProtocol; }
        uint8_t getMaxPacketSize0() const { return desc_.bMaxPacketSize0; }
        uint16_t getVendorID() const { return desc_.idVendor; }
        uint16_t getProductID() const { return desc_.idProduct; }
        uint16_t getDeviceVersion() const { return desc_.bcdDevice; }
        uint8_t getManufacturerStringIndex() const { return desc_.iManufacturer; }
        uint8_t getProductStringIndex() const { return desc_.iProduct; }
        uint8_t getSerialStringIndex() const { return desc_.iSerialNumber; }
        uint8_t getNumConfigurations() const { return desc_.bNumConfigurations; }

        // Setters
        void setUSBVersion(uint16_t version) { desc_.bcdUSB = version; }
        void setDeviceClass(uint8_t deviceClass) { desc_.bDeviceClass = deviceClass; }
        void setDeviceSubClass(uint8_t subClass) { desc_.bDeviceSubClass = subClass; }
        void setDeviceProtocol(uint8_t protocol) { desc_.bDeviceProtocol = protocol; }
        void setMaxPacketSize0(uint8_t maxPacketSize) { desc_.bMaxPacketSize0 = maxPacketSize; }
        void setVendorID(uint16_t vendorId) { desc_.idVendor = vendorId; }
        void setProductID(uint16_t productId) { desc_.idProduct = productId; }
        void setDeviceVersion(uint16_t version) { desc_.bcdDevice = version; }
        void setManufacturerStringIndex(uint8_t index) { desc_.iManufacturer = index; }
        void setProductStringIndex(uint8_t index) { desc_.iProduct = index; }
        void setSerialStringIndex(uint8_t index) { desc_.iSerialNumber = index; }

        bool addConfiguration(const ConfigurationDescriptor &config);
        const std::vector<ConfigurationDescriptor> &getConfigurations() const { return configurations_; }
        std::vector<uint8_t> generateDescriptorBlock() const;

        operator tusb_desc_device_t() const { return desc_; }

    protected:
        tusb_desc_device_t desc_;
        std::vector<ConfigurationDescriptor> configurations_;

        void updateDescriptor();
    };

} // namespace USB

#endif // USB_DESCRIPTOR_H_