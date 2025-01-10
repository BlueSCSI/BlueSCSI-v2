// Copyright (C) 2024 akuker
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

#include <algorithm>

#include "usb_descriptors.h"

namespace USB
{
    NumberManager InterfaceDescriptor::number_manager;

    void InterfaceDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        BasicDescriptor::addChildDescriptor(child);
        if (child->getDescriptorType() == TUSB_DESC_ENDPOINT)
        {
            desc_.bNumEndpoints++;
        }
    }

    void InterfaceAssociationDescriptor::addChildDescriptor(BasicDescriptor *child)
    {
        if (child->getDescriptorType() != TUSB_DESC_INTERFACE)
        {
            log("ERROR: InterfaceAssociationDescriptor::addChildDescriptor: child is not InterfaceDescriptor");
        }

        BasicDescriptor::addChildDescriptor(child);
        if (desc_.bFirstInterface == 0xFF)
        {
            desc_.bFirstInterface = static_cast<InterfaceDescriptor *>(child)->getInterfaceNumber();
        }
        desc_.bInterfaceCount++;
    }

} // namespace USB