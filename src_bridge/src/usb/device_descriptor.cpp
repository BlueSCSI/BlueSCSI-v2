#include "usb_descriptors.h"
#include <algorithm>
namespace USB
{

    void DeviceDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        BasicDescriptor::addChildDescriptor(child);
        desc_.bNumConfigurations = static_cast<uint8_t>(getChildDescriptors().size());
    }

} // namespace USB