#include "usb_descriptors.h"
#include <algorithm>

USB::NumberManager USB::InterfaceDescriptor::number_manager;

namespace USB
{

   void InterfaceDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        BasicDescriptor::addChildDescriptor(child);
        if(child->getDescriptorType() == TUSB_DESC_ENDPOINT)
        {
            desc_.bNumEndpoints++;
        }
    }


    void InterfaceAssociationDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        if(child->getDescriptorType() != TUSB_DESC_INTERFACE){
            log("ERROR: InterfaceAssociationDescriptor::addChildDescriptor: child is not InterfaceDescriptor");
        }

        BasicDescriptor::addChildDescriptor(child);
        if(desc_.bFirstInterface == 0xFF)
        {
            desc_.bFirstInterface = static_cast<InterfaceDescriptor*>(child)->getInterfaceNumber();
        }
        desc_.bInterfaceCount++;
    }

    // void InterfaceAssociationDescriptor::generateDescriptorBlock(){
    //     desc_.bLength = sizeof(desc_);
    //     desc_.bDescriptorType = DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION;
    //     desc_.bNumInterfaces = static_cast<uint8_t>(getChildDescriptors().size());
    //     asdf
    // }

} // namespace USB