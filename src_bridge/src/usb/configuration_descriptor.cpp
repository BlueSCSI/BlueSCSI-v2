#include "usb_descriptor.h"
#include <algorithm>

namespace USB
{

    bool ConfigurationDescriptor::addInterface(const InterfaceDescriptor &interface)
    {
        // Check for duplicate interface numbers
        for (const auto &existing : interfaces_)
        {
            if (existing.getInterfaceNumber() == interface.getInterfaceNumber())
            {
                return false; // Interface number already exists
            }
        }

        interfaces_.push_back(interface);
        updateDescriptor();
        return true;
    }

    const InterfaceDescriptor *ConfigurationDescriptor::getInterface(uint8_t interfaceNumber) const
    {
        auto it = std::find_if(interfaces_.begin(), interfaces_.end(),
                               [interfaceNumber](const InterfaceDescriptor &iface)
                               {
                                   return iface.getInterfaceNumber() == interfaceNumber;
                               });

        return (it != interfaces_.end()) ? &(*it) : nullptr;
    }

    std::vector<uint8_t> ConfigurationDescriptor::generateDescriptorBlock() const
    {
        std::vector<uint8_t> block;

        // Configuration descriptor
        block.insert(block.end(), (uint8_t *)&desc_, (uint8_t *)(&desc_ + 1));

        // Add each interface and its endpoints
        for (const auto &interface : interfaces_)
        {
            // Interface descriptor
            const tusb_desc_interface_t &iface = interface;
            block.insert(block.end(), (uint8_t *)&iface, (uint8_t *)(&iface + 1));

            // Add endpoints for this interface
            for (const auto &endpoint : interface.getEndpoints())
            {
                const tusb_desc_endpoint_t &ep = endpoint;
                block.insert(block.end(), (uint8_t *)&ep, (uint8_t *)(&ep + 1));
            }
        }

        // Update total length in configuration descriptor
        if (block.size() >= 2)
        {
            uint16_t totalLength = static_cast<uint16_t>(block.size());
            block[2] = TU_U16_LOW(totalLength);
            block[3] = TU_U16_HIGH(totalLength);
        }

        return block;
    }

    void ConfigurationDescriptor::updateDescriptor()
    {
        desc_.bNumInterfaces = static_cast<uint8_t>(interfaces_.size());

        // Calculate total length
        desc_.wTotalLength = sizeof(tusb_desc_configuration_t);
        for (const auto &interface : interfaces_)
        {
            desc_.wTotalLength += sizeof(tusb_desc_interface_t);
            desc_.wTotalLength += static_cast<uint16_t>(interface.getEndpoints().size() * sizeof(tusb_desc_endpoint_t));
        }
    }

} // namespace USB