#include "usb_descriptors.h"
#include <algorithm>
#include "BlueSCSI_log.h"

namespace USB
{

    NumberManager ConfigurationDescriptor::number_manager;

    void ConfigurationDescriptor::addChildDescriptor(BasicDescriptor *interface)
    {
        // // Check for duplicate interface numbers
        //////// Not all class-specific interfaces have assigned interface numbers
        // for (const auto &existing : interfaces_)
        // {
        //     if (existing->getInterfaceNumber() == interface->getInterfaceNumber())
        //     {
        //         log("WARNING!!! DUPLICATE INTERFACE NUMBER!!!");
        //         // return false; // Interface number already exists
        //         // I think this is OK. There are interfaces in the CDC configurati
        //     }
        // }

        getChildDescriptors().push_back(interface);
        // Only include the interfaces that can have endpoints
        if (interface->supportsChildren())
        {
            desc_.bNumInterfaces++;
        }
    }

    // const InterfaceDescriptor *ConfigurationDescriptor::getInterface(uint8_t interfaceNumber) const
    // {
    //     auto it = std::find_if(interfaces_.begin(), interfaces_.end(),
    //                            [interfaceNumber](const InterfaceDescriptor &iface)
    //                            {
    //                                return iface.getInterfaceNumber() == interfaceNumber;
    //                            });

    //     return (it != interfaces_.end()) ? &(*it) : nullptr;
    // }

    const size_t ConfigurationDescriptor::getDescriptorSizeBytes()
    {
        // Calculate total length
        desc_.wTotalLength = sizeof(tusb_desc_configuration_t);
        for (BasicDescriptor* interface : getChildDescriptors())
        {
            desc_.wTotalLength += interface->getDescriptorSizeBytes();
        }
        return (size_t)desc_.wTotalLength;
    }

    // std::vector<uint8_t> ConfigurationDescriptor::generateDescriptorBlock()
    // {

    //     return BasicDescriptor::generateDescriptorBlock();
    // }
//         // TODO......


// countChildrenOfType

//         // Check that total length matches what we expected
//         if (block.size() != desc_.wTotalLength)
//         {
//             log("ERROR: Configuration descriptor size mismatch");
//         }

//         // // Update total length in configuration descriptor
//         // if (block.size() >= 2)
//         // {
//         //     uint16_t totalLength = static_cast<uint16_t>(block.size());
//         //     block[2] = TU_U16_LOW(totalLength);
//         //     block[3] = TU_U16_HIGH(totalLength);
//         // }

//         return block;
//         // return std::vector<uint8_t>();
//     }

} // namespace USB