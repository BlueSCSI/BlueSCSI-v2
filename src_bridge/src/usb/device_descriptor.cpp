#include "usb_descriptors.h"
#include <algorithm>
namespace USB
{

    void DeviceDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        BasicDescriptor::addChildDescriptor(child);
        desc_.bNumConfigurations = static_cast<uint8_t>(getChildDescriptors().size());
    }

    // std::vector<uint8_t> DeviceDescriptor::generateDescriptorBlock() const
    // {
    //     std::vector<uint8_t> block;
    //     block.reserve(sizeof(tusb_desc_device_t));

    //     // Copy the device descriptor
    //     block.insert(block.end(), (uint8_t *)&desc_, (uint8_t *)(&desc_ + 1));

    //     return block;
    // }

} // namespace USB