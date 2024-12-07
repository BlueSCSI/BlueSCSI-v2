#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <list>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include "tusb.h"
#include <BlueSCSI_log.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include <climits>
#include "bsp/board_api.h"
// #include "tusb_common.h"

// temporary
extern "C"
{
    void save_descriptor(const char *name, const uint8_t *buf, size_t len);
    std::string get_desc_block_type(tusb_desc_type_t mytype);
}

namespace USB
{

    class StringDescriptor
    {
    public:
        // String length is limited by the size of the field (1 byte) minus the length byte
        static const int MAX_STR_LEN_ = UINT8_MAX - 1;
        // USB String Descriptor, but with fixed (maximum) size
        typedef struct TU_ATTR_PACKED
        {
            uint8_t bLength;         ///< Size of this descriptor in bytes
            uint8_t bDescriptorType; ///< Descriptor Type
            uint16_t unicode_string[MAX_STR_LEN_];
        } tusb_desc_string_fixed_len_t;

        StringDescriptor()
        {
            mutex_ = xSemaphoreCreateMutex();
            configASSERT(mutex_);
        }
        ~StringDescriptor()
        {
            vSemaphoreDelete(mutex_);
            string_list_.clear();
        }
        uint8_t addString(std::string new_string)
        {
            // If the string is too long, we need to truncate it
            if (new_string.length() > MAX_STR_LEN_)
            {
                new_string = new_string.substr(0, MAX_STR_LEN_);
            }

            // xSemaphoreTake(mutex_, portMAX_DELAY);
            uint8_t string_location = findStringIdx(new_string);
            if (string_location == UINT8_MAX)
            {
                // If the string isn't already there, add it.
                string_list_.push_back(new_string);
                // find it again
                string_location = findStringIdx(new_string);
            }
            // xSemaphoreGive(mutex_);
            if (string_location == UINT8_MAX)
            {
                log("Warning!! Too many string indices added to StringDescriptor");
                return string_location;
            }
            else
            {
                return string_location;
            }
        }

        std::string getString(uint8_t string_idx)
        {
            if (string_idx < string_list_.size())
            {
                return string_list_[string_idx];
            }
            else
            {
                return "";
            }
        }

        uint8_t findStringIdx(std::string string) const
        {
            auto string_location = std::find(string_list_.begin(), string_list_.end(), string);
            if (string_location == string_list_.end())
            {
                // Not found
                return UINT8_MAX;
            }
            else
            {
                int index = std::distance(string_list_.begin(), string_location);
                if (index > UINT8_MAX)
                {
                    log("Warning!! Too many string indices added to StringDescriptor");
                    return UINT8_MAX;
                }
                else
                {
                    return (uint8_t)index;
                }
            }
        }

        std::shared_ptr<tusb_desc_string_fixed_len_t> generateDescriptor(uint8_t index)
        {

            // Special case for "0", which is the language ID
            if (index == 0)
            {
                auto fixed_desc = std::make_shared<tusb_desc_string_fixed_len_t>();
                fixed_desc->bLength = 4;
                fixed_desc->bDescriptorType = TUSB_DESC_STRING;
                fixed_desc->unicode_string[0] = 0x0409;
                return fixed_desc;
            }

            xSemaphoreTake(mutex_, portMAX_DELAY);

            if (index < string_list_.size())
            {
                std::string the_string = string_list_[index];
                xSemaphoreGive(mutex_);

                // Calculate the size of the descriptor block
                //    bLength (uint8_t) + bDescriptorType(uint8_t) + string converted to unicode
                int desc_length = sizeof(uint8_t) + sizeof(uint8_t) + (2 * the_string.length());

                // std::shared_ptr<tusb_desc_string_t> desc
                auto fixed_desc = std::make_shared<tusb_desc_string_fixed_len_t>();
                fixed_desc->bLength = desc_length;
                fixed_desc->bDescriptorType = TUSB_DESC_STRING;
                // Convert to 16-bit unicode
                size_t str_len = the_string.length();

                // Convert ASCII string into UTF-16
                for (size_t i = 0; i < str_len; i++)
                {
                    fixed_desc->unicode_string[i] = (uint16_t)the_string[i];
                }
                xSemaphoreGive(mutex_);

                return fixed_desc;
            }
            else
            {
                xSemaphoreGive(mutex_);
                // Invalid string index requested
                auto inv_desc = std::make_shared<tusb_desc_string_fixed_len_t>();
                inv_desc->bLength = 0;
                inv_desc->bDescriptorType = TUSB_DESC_STRING;
                return inv_desc;
            }
        }

    protected:
        std::vector<std::string> string_list_;
        SemaphoreHandle_t mutex_;

        // Reference: https://stackoverflow.com/questions/22548300/shared-ptr-to-variable-length-struct
        // C++20 polyfill for missing "make_shared<T[]>(size_t size)" overload.
        template <typename T>
        inline std::shared_ptr<T> make_shared_array(size_t bufferSize)
        {
            return std::shared_ptr<T>(new T[bufferSize], [](T *memory)
                                      { delete[] memory; });
        }

        // Creates a smart pointer to a type backed by a variable length buffer, e.g. system structures.
        template <typename T>
        inline std::shared_ptr<T> CreateSharedBuffer(size_t byteSize)
        {
            return std::reinterpret_pointer_cast<T>(make_shared_array<uint8_t>(byteSize));
        }
    };

    extern StringDescriptor g_usb_string_descriptor;

    // Generic number manager for assigning unique interface/endpoint numbers
    class NumberManager
    {
    public:
        NumberManager()
        {
            mutex = xSemaphoreCreateMutex();
            configASSERT(mutex);
        }

        ~NumberManager()
        {
            vSemaphoreDelete(mutex);
        }
        uint8_t assignNumber(int starting_num = 0)
        {
            uint8_t interface_num = starting_num;
            xSemaphoreTake(mutex, portMAX_DELAY);
            // loop until we find the first unused interface number
            while (std::find(interface_num_list.begin(), interface_num_list.end(), interface_num) != interface_num_list.end())
            {
                interface_num++;
            }
            interface_num_list.push_back(interface_num);
            xSemaphoreGive(mutex);
            return interface_num;
        }

    private:
        SemaphoreHandle_t mutex;
        std::vector<uint8_t> interface_num_list;
    };

    class BasicDescriptor
    {
    public:
        // Abstract class that is used as the basis for the specific
        // descriptors
        virtual ~BasicDescriptor() = default;

        virtual const bool supportsChildren() const = 0;
        virtual const size_t getDescriptorSizeBytes() { 
            size_t calc_size = _getDescriptorSizeBytes();
            for (BasicDescriptor* child : getChildDescriptors())
            {
                calc_size += child->getDescriptorSizeBytes();
            }
            return calc_size;
        }
        virtual std::vector<uint8_t> generateDescriptorBlock()
        {

            printf("Generating descriptor block for %s\n", get_desc_block_type(getDescriptorType()).c_str());

            std::vector<uint8_t> block;

            std::vector<uint8_t> desc_block = _getDescriptorBlockBytes();
            // Configuration descriptor
            block.insert(block.end(), desc_block.begin(), desc_block.end());

            volatile auto block_size = block.size();

            // Add each interface and its endpoints
            for (const auto child : getChildDescriptors())
            {
                volatile auto mytype = child->getDescriptorType();
                if (mytype == TUSB_DESC_INTERFACE_ASSOCIATION)
                {
                    printf("Adding interface association descriptor\n");
                }
                if (mytype == TUSB_DESC_CONFIGURATION)
                {
                    printf("Adding configuration descriptor\n");
                }
                // Interface descriptor
                std::vector<uint8_t> child_block = child->generateDescriptorBlock();
                block.insert(block.end(), child_block.begin(), child_block.end());
                block_size = block.size();
            }
            return block;
        }
        virtual void addChildDescriptor(BasicDescriptor *child) { child_descriptors_.push_back(child); };
        int countChildrenOfType(tusb_desc_type_t type)
        {
            int count = 0;
            if (getDescriptorType() == type)
            {
                count++;
            }
            for (const auto child : getChildDescriptors())
            {
                volatile tusb_desc_type_t mytype = child->getDescriptorType();
                count += child->countChildrenOfType(type);
            }
            return count;
        }

        std::vector<BasicDescriptor *> &getChildDescriptors() { return child_descriptors_; }
        const size_t getNumChildrenDescriptors() const { return child_descriptors_.size(); }
        virtual const tusb_desc_type_t getDescriptorType() const = 0;

    protected:
        // Prevent this class from being instantiated on its own
        BasicDescriptor() = default;

        std::vector<BasicDescriptor *> child_descriptors_;
        virtual std::vector<uint8_t> _getDescriptorBlockBytes() = 0;
        virtual size_t _getDescriptorSizeBytes() const = 0;
    };

    class EndpointDescriptor : public BasicDescriptor
    {
    public:
        enum class Attributes : uint8_t
        {
            XFER_TYPE_CONTROL = 0b00000000,
            XFER_TYPE_ISO = 0b00000001,
            XFER_TYPE_BULK = 0b00000010,
            XFER_TYPE_INTERRUPT = 0b00000011,
            SYNC_TYPE_NONE = 0b00000000,
            SYNC_TYPE_ASYNC = 0b00000100,
            SYNC_TYPE_ADAPTIVE = 0b00001000,
            SYNC_TYPE_SYNC = 0b00001100,
            USAGE_TYPE_DATA = 0b00000000,
            USAGE_TYPE_FEEDBACK = 0b00010000,
            USAGE_TYPE_IMPLICIT_FEEDBACK_DATA = 0b00100000,
            USAGE_TYPE_RESERVED = 0b00110000,
        };

        EndpointDescriptor(tusb_dir_t direction)
        {
            desc_.bLength = sizeof(tusb_desc_endpoint_t);
            desc_.bDescriptorType = getDescriptorType();
            // If the direction is "IN" we need to assign a number with the MSB set
            if (direction == TUSB_DIR_IN)
            {
                desc_.bEndpointAddress = EndpointDescriptor::number_manager.assignNumber(TUSB_DIR_IN_MASK);
            }
            else
            {
                desc_.bEndpointAddress = EndpointDescriptor::number_manager.assignNumber();
            }
            attr_conv_union_t attr;
            attr.enum_attr = Attributes::XFER_TYPE_BULK;
            desc_.bmAttributes = attr.attributes;
            desc_.bInterval = 0;
            desc_.wMaxPacketSize = 0;
        }

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
        tusb_dir_t getDirection() const { return (tusb_dir_t)(desc_.bEndpointAddress & TUSB_DIR_IN_MASK); }
        // const size_t getDescriptorSizeBytes() override { return sizeof(tusb_desc_endpoint_t); }
        const bool supportsChildren() const override { return false; };
        const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_ENDPOINT; };

        // Setters
        void setMaxPacketSize(uint16_t size) { desc_.wMaxPacketSize = size; }
        void setInterval(uint8_t interval) { desc_.bInterval = interval; }
        void setTransferType(tusb_xfer_type_t type) { desc_.bmAttributes.xfer = type; }

        void setAttributes(uint8_t attributes)
        {
            desc_.bmAttributes.xfer = (attributes & 0x03);
            desc_.bmAttributes.sync = (attributes & 0x0C) >> 2;
            desc_.bmAttributes.usage = (attributes & 0x30) >> 4;
        }
        void setAttributes(Attributes attributes)
        {
            attr_conv_union_t attr;
            attr.enum_attr = attributes;
            desc_.bmAttributes = attr.attributes;
        }

        operator tusb_desc_endpoint_t() const { return desc_; }

    protected:
        tusb_desc_endpoint_t desc_;
        typedef union
        {
            uint8_t raw;
            Attributes enum_attr;
            decltype(tusb_desc_endpoint_t::bmAttributes) attributes;
        } attr_conv_union_t;
        static NumberManager number_manager;
        std::vector<uint8_t> _getDescriptorBlockBytes()
        {
            return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
        }
        size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
    };

    class InterfaceDescriptor : public BasicDescriptor
    {
    public:
        InterfaceDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_interface_t);
            desc_.bDescriptorType = getDescriptorType();
            desc_.bInterfaceNumber = InterfaceDescriptor::number_manager.assignNumber();
            desc_.bAlternateSetting = 0;
            desc_.bNumEndpoints = 0;
            desc_.bInterfaceClass = 0;
            desc_.bInterfaceSubClass = 0;
            desc_.bInterfaceProtocol = 0;
            desc_.iInterface = 0;
        }

        // explicit InterfaceDescriptor(const tusb_desc_interface_t &desc) : desc_(desc) {}

        // Getters
        const bool supportsChildren() const override { return true; };

        uint8_t getInterfaceNumber() const { return desc_.bInterfaceNumber; }
        uint8_t getAlternateSetting() const { return desc_.bAlternateSetting; }
        uint8_t getNumEndpoints() const { return desc_.bNumEndpoints; }
        uint8_t getInterfaceClass() const { return desc_.bInterfaceClass; }
        uint8_t getInterfaceSubClass() const { return desc_.bInterfaceSubClass; }
        uint8_t getInterfaceProtocol() const { return desc_.bInterfaceProtocol; }
        uint8_t getNameIdx() const { return desc_.iInterface; }
        std::string getName() const { return g_usb_string_descriptor.getString(desc_.iInterface); }
        const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_INTERFACE; };

        // Setters
        // Interface number should be set automatically
        // void setInterfaceNumber(uint8_t num) { desc_.bInterfaceNumber = num; }
        void setAlternateSetting(uint8_t alt) { desc_.bAlternateSetting = alt; }
        // Should be automatically set
        // void setNumEndpoints(uint8_t num) { desc_.bNumEndpoints = num; }
        void setInterfaceClass(uint8_t cls) { desc_.bInterfaceClass = cls; }
        void setInterfaceSubClass(uint8_t subcls) { desc_.bInterfaceSubClass = subcls; }
        void setInterfaceProtocol(uint8_t protocol) { desc_.bInterfaceProtocol = protocol; }
        // void setStringIndex(uint8_t index) { desc_.iInterface = index; }
        void setName(const std::string &name)
        {
            desc_.iInterface = g_usb_string_descriptor.addString(name);
        }

        void addChildDescriptor(BasicDescriptor *child) override;

        // const size_t getDescriptorSizeBytes() override { return sizeof(tusb_desc_interface_t); }

        operator tusb_desc_interface_t() const { return desc_; }

    protected:
        tusb_desc_interface_t desc_;
        // std::vector<const BasicDescriptor *> functional_descriptors_;
        static NumberManager number_manager;
        std::vector<uint8_t> _getDescriptorBlockBytes() override
        {
            desc_.bNumEndpoints = countChildrenOfType(TUSB_DESC_ENDPOINT);
            return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
        }
        size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
    };

    class InterfaceAssociationDescriptor : public BasicDescriptor
    {
    public:
        InterfaceAssociationDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_interface_assoc_t);
            desc_.bDescriptorType = getDescriptorType();
            desc_.bFirstInterface = 0xFF;
            desc_.bInterfaceCount = 0;
            desc_.bFunctionClass = 0;
            desc_.bFunctionSubClass = 0;
            desc_.bFunctionProtocol = 0;
            desc_.iFunction = 0;
        }

        // Setters
        void setFunctionClass(uint8_t cls) { desc_.bFunctionClass = cls; }
        void setFunctionSubClass(uint8_t subcls) { desc_.bFunctionSubClass = subcls; }
        void setFunctionProtocol(uint8_t protocol) { desc_.bFunctionProtocol = protocol; }
        void setFunction(std::string fn_name)
        {
            int index = g_usb_string_descriptor.addString(fn_name);
            desc_.iFunction = index;
        }
        void addChildDescriptor(BasicDescriptor *child) override;

        uint8_t getInterfaceNumber() const { return desc_.bFirstInterface; }
        uint8_t getFunctionClass() { return desc_.bFunctionClass; }
        uint8_t getFunctionSubClass() { return desc_.bFunctionSubClass; }
        uint8_t getFunctionProtocol() { return desc_.bFunctionProtocol; }
        std::string getFunction() { return g_usb_string_descriptor.getString(desc_.iFunction); }
        const bool supportsChildren() const override { return true; }
        const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_INTERFACE_ASSOCIATION; }
        // const size_t getDescriptorSizeBytes() override { return sizeof(tusb_desc_interface_assoc_t); }

    protected:
        tusb_desc_interface_assoc_t desc_;
        // std::vector<const InterfaceDescriptor *> interfaces_;
        std::vector<uint8_t> _getDescriptorBlockBytes() override
        {
            return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
        }
        size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
    };

    namespace CDC
    {

        class HeaderFunctionalDescriptor : public BasicDescriptor
        {
        public:
            HeaderFunctionalDescriptor()
            {
                desc_.bLength = sizeof(cdc_desc_func_header_t);
                desc_.bDescriptorType = getDescriptorType();
                desc_.bDescriptorSubType = CDC_FUNC_DESC_HEADER;
                desc_.bcdCDC = 0x0120;
            }
            explicit HeaderFunctionalDescriptor(const cdc_desc_func_header_t &desc) : desc_(desc) {}

            void setBcdCdc(uint16_t bcd) { desc_.bcdCDC = bcd; }
            uint16_t getBcdCdc() { return desc_.bcdCDC; }
            // const size_t getDescriptorSizeBytes() override { return sizeof(cdc_desc_func_header_t); }
            const bool supportsChildren() const override { return false; }
            const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_CS_INTERFACE; }

        protected:
            cdc_desc_func_header_t desc_;
            std::vector<uint8_t> _getDescriptorBlockBytes() override
            {
                return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
            }
            size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
        };

        class UnionFunctionalDescriptor : public BasicDescriptor
        {
        public:
            UnionFunctionalDescriptor()
            {
                desc_.bLength = sizeof(cdc_desc_func_union_t);
                desc_.bDescriptorType = getDescriptorType();
                desc_.bDescriptorSubType = CDC_FUNC_DESC_UNION;
                desc_.bControlInterface = 0;
                desc_.bSubordinateInterface = 0;
            }

            void setSubtype(uint8_t subtype) { desc_.bDescriptorSubType = subtype; }
            void setControlInterface(uint8_t interface) { desc_.bControlInterface = interface; }
            void setSubordinateInterface(uint8_t interface) { desc_.bSubordinateInterface = interface; }

            uint8_t getSubtype() { return desc_.bDescriptorSubType; }
            uint8_t getControlInterface() { return desc_.bControlInterface; }
            uint8_t getSubordinateInterface() { return desc_.bSubordinateInterface; }
            // const size_t getDescriptorSizeBytes() override { return sizeof(cdc_desc_func_union_t); }
            const bool supportsChildren() const override { return false; }
            const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_CS_INTERFACE; }

        private:
            cdc_desc_func_union_t desc_;
            std::vector<uint8_t> _getDescriptorBlockBytes() override
            {
                return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
            }
            size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
        };

        class CallManagementFunctionalDescriptor : public BasicDescriptor
        {
        public:
            CallManagementFunctionalDescriptor()
            {
                desc_.bLength = sizeof(cdc_desc_func_call_management_t);
                desc_.bDescriptorType = getDescriptorType();
                desc_.bDescriptorSubType = CDC_FUNC_DESC_CALL_MANAGEMENT;
                desc_.bmCapabilities = {0};
                desc_.bDataInterface = 0; // InterfaceDescriptor::number_manager.assignNumber();
            }

            void setHandleCall(bool handle) { desc_.bmCapabilities.handle_call = handle; }
            void setSendRecvCall(bool send_recv) { desc_.bmCapabilities.send_recv_call = send_recv; }
            void setDataInterface(uint8_t interface) { desc_.bDataInterface = interface; }

            bool getHandleCall() { return desc_.bmCapabilities.handle_call; }
            bool getSendRecvCall() { return desc_.bmCapabilities.send_recv_call; }
            uint8_t getDataInterface() { return desc_.bDataInterface; }
            // uint8_t getInterfaceNumber() { return desc_.bDataInterface; }
            // const size_t getDescriptorSizeBytes() override { return sizeof(cdc_desc_func_call_management_t); }
            const bool supportsChildren() const override { return false; }
            const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_CS_INTERFACE; }

            // std::vector<uint8_t> generateDescriptorBlock() const override
            // {
            //     return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(cdc_desc_func_call_management_t));
            // }

        protected:
            cdc_desc_func_call_management_t desc_;
            std::vector<uint8_t> _getDescriptorBlockBytes() override
            {
                return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
            }
            size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
        };

        class AbstractControlManagementFunctionalDescriptor : public BasicDescriptor
        {
        public:
            AbstractControlManagementFunctionalDescriptor()
            {
                desc_.bLength = sizeof(cdc_desc_func_acm_t);
                desc_.bDescriptorType = getDescriptorType();
                desc_.bDescriptorSubType = CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT;
                desc_.bmCapabilities.support_comm_request = 0;
                desc_.bmCapabilities.support_line_request = 0;
                desc_.bmCapabilities.support_send_break = 0;
                desc_.bmCapabilities.support_notification_network_connection = 0;
            }

            void setSupportCommRequest(bool support) { desc_.bmCapabilities.support_comm_request = support; }
            void setSupportLineRequest(bool support) { desc_.bmCapabilities.support_line_request = support; }
            void setSupportSendBreak(bool support) { desc_.bmCapabilities.support_send_break = support; }
            void setSupportNotificationNetworkConnection(bool support) { desc_.bmCapabilities.support_notification_network_connection = support; }

            bool getSupportCommRequest() { return desc_.bmCapabilities.support_comm_request; }
            bool getSupportLineRequest() { return desc_.bmCapabilities.support_line_request; }
            bool getSupportSendBreak() { return desc_.bmCapabilities.support_send_break; }
            bool getSupportNotificationNetworkConnection() { return desc_.bmCapabilities.support_notification_network_connection; }
            // const size_t getDescriptorSizeBytes() override { return sizeof(cdc_desc_func_acm_t); }
            const bool supportsChildren() const override { return false; }
            const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_CS_INTERFACE; }

            // std::vector<uint8_t> generateDescriptorBlock() const override
            // {
            //     return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(cdc_desc_func_acm_t));
            // }

        protected:
            cdc_desc_func_acm_t desc_;
            std::vector<uint8_t> _getDescriptorBlockBytes() override
            {
                return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
            }
            size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
        };
    }

    class ConfigurationDescriptor : public BasicDescriptor
    {
    public:
        ConfigurationDescriptor()
        {
            desc_.bLength = sizeof(tusb_desc_configuration_t);
            desc_.bDescriptorType = getDescriptorType();
            desc_.bNumInterfaces = 0;
            // Configuration numbers should start at "1". (There probably will only
            // ever be 1 configuration value, so..... whatever.)
            desc_.bConfigurationValue = number_manager.assignNumber(1);
            desc_.bmAttributes = TU_BIT(7);
            desc_.bMaxPower = TUSB_DESC_CONFIG_POWER_MA(500);
            desc_.iConfiguration = 0;
        }

        explicit ConfigurationDescriptor(const tusb_desc_configuration_t &desc) : desc_(desc) {}

        // Getters
        uint16_t getTotalLength() const { return desc_.wTotalLength; }
        uint8_t getNumInterfaces() const { return desc_.bNumInterfaces; }
        uint8_t getConfigurationValue() const { return desc_.bConfigurationValue; }
        uint8_t getNameIdx() const { return desc_.iConfiguration; }
        std::string getName() const { return g_usb_string_descriptor.getString(desc_.iConfiguration); }
        uint8_t getAttributes() const { return desc_.bmAttributes; }
        // There isn't a macro for performing the inverse of TUSB_DESC_CONFIG_POWER_MA, so this will
        // need to be manually maintained.
        TU_VERIFY_STATIC(TUSB_DESC_CONFIG_POWER_MA(100) == 50, "TUSB_DESC_CONFIG_POWER_MA macro definition has changed");
        uint8_t getMaxPower() const { return desc_.bMaxPower * 2; }
        const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_CONFIGURATION; }
        const bool supportsChildren() const override { return true; }

        // Setters
        // void setConfigurationValue(uint8_t value) { desc_.bConfigurationValue = value; }
        void setAttributes(uint8_t attributes) { desc_.bmAttributes = attributes; }
        void setSelfPowered(bool selfPowered)
        {
            if (selfPowered)
            {
                desc_.bmAttributes |= TUSB_DESC_CONFIG_ATT_SELF_POWERED;
            }
            else
            {
                desc_.bmAttributes &= ~TUSB_DESC_CONFIG_ATT_SELF_POWERED;
            }
        }
        void setRemoteWakeup(bool remoteWakeup)
        {
            if (remoteWakeup)
            {
                desc_.bmAttributes |= TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP;
            }
            else
            {
                desc_.bmAttributes &= ~TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP;
            }
        }
        void setMaxPower(uint8_t maxPower) { desc_.bMaxPower = TUSB_DESC_CONFIG_POWER_MA(maxPower); }
        void setName(const std::string &name)
        {
            desc_.iConfiguration = g_usb_string_descriptor.addString(name);
        }

        // bool addInterface(const InterfaceDescriptor *interface);
        // const InterfaceDescriptor *getInterface(uint8_t interfaceNumber) const;

        void addChildDescriptor(BasicDescriptor *interface) override;

        // const bool supportsChildren() const override { return true; }
        // std::vector<uint8_t> generateDescriptorBlock() override;
        const size_t getDescriptorSizeBytes() override;

        operator tusb_desc_configuration_t() const { return desc_; }

    protected:
        tusb_desc_configuration_t desc_;
        static NumberManager number_manager;
        std::vector<uint8_t> _getDescriptorBlockBytes() override
        {
            volatile auto num_children = getChildDescriptors().size();
            desc_.bNumInterfaces = (uint8_t)BasicDescriptor::countChildrenOfType(TUSB_DESC_INTERFACE);
            desc_.wTotalLength = getDescriptorSizeBytes();
            return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
        }
        size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }
    };

    class DeviceDescriptor : public BasicDescriptor
    {
    public:
        DeviceDescriptor(std::string manufacturer = "TinyUSB", std::string product = "TinyUSB Device", std::string serial_num = "")
        {
            desc_.bLength = sizeof(tusb_desc_device_t);
            desc_.bDescriptorType = getDescriptorType();
            desc_.bNumConfigurations = 0;

            // First string needs to be language ID
            g_usb_string_descriptor.addString("LANGUAGEID");
            // Second string is manufacturer
            g_usb_string_descriptor.addString(manufacturer);
            // Third string is product
            g_usb_string_descriptor.addString(product);
            // Fourth string is serial number
            g_usb_string_descriptor.addString(getSerialNumber());
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
        const bool supportsChildren() const { return true; }
        const tusb_desc_type_t getDescriptorType() const override { return TUSB_DESC_DEVICE; }

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

        void addChildDescriptor(BasicDescriptor *child) override;

        // bool addConfiguration(const ConfigurationDescriptor *config);
        // const std::vector<ConfigurationDescriptor> &getConfigurations() const { return configurations_; }
        const size_t getDescriptorSizeBytes() override { return sizeof(tusb_desc_device_t); }
        // std::vector<uint8_t> generateDescriptorBlock() const override;

        operator tusb_desc_device_t() const { return desc_; }
        // // uint8_t *getDescriptorBlock() const { return (uint8_t *)&desc_; }
        // ssize_t getDescriptorBlockLength() const { return sizeof(tusb_desc_device_t); }

    protected:
        tusb_desc_device_t desc_;
        std::vector<uint8_t> _getDescriptorBlockBytes() override
        {
            return std::vector<uint8_t>((uint8_t *)&desc_, ((uint8_t *)&desc_) + sizeof(desc_));
        }
        size_t _getDescriptorSizeBytes() const override { return sizeof(desc_); }

        std::string getSerialNumber()
        {
            uint16_t desc_str[StringDescriptor::MAX_STR_LEN_];
            size_t unicode_char_count = board_usb_get_serial((uint16_t *)desc_str, sizeof(desc_str) / 2);
            // The returned serial number will be hex numbers, so we can just convert
            // those to 8 bit characters
            char desc_str_char[unicode_char_count + 1];
            for (size_t i = 0; i < unicode_char_count; i++)
            {
                desc_str_char[i] = (char)(desc_str[i] & 0xFF);
            }
            desc_str_char[unicode_char_count] = '\0';
            return std::string(desc_str_char);
        }
    };

} // namespace USB
