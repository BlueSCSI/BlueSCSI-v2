#include "usb_descriptors.h"
#include <algorithm>

namespace USB
{

    static NumberManager g_interface_number_manager;

    void InterfaceDescriptor::addEndpoint(const EndpointDescriptor *ep)
    {
        endpoints_.push_back(ep);
        desc_.bNumEndpoints = static_cast<uint8_t>(endpoints_.size());
    }

} // namespace USB