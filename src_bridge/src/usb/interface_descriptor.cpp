#include "usb_descriptor.h"
#include <algorithm>

namespace USB {

bool InterfaceDescriptor::addEndpoint(const EndpointDescriptor& ep) {
    endpoints_.push_back(ep);
    desc_.bNumEndpoints = static_cast<uint8_t>(endpoints_.size());
    return true;
}

} // namespace USB