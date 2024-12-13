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

#include "usb_descriptors.h"

extern bool g_disable_usb_cdc;
extern bool g_scsi_msc_mode;

#define USB_VID 0x1209
#define USB_BCD 0xB551
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                 _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))

static void build_cdc_interface(USB::ConfigurationDescriptor *cfg_desc);
static void build_msc_interface(USB::ConfigurationDescriptor *cfg_desc);

static USB::DeviceDescriptor *device_desc;
static USB::ConfigurationDescriptor *config_desc;

static void build_device_descriptor()
{
    device_desc = new USB::DeviceDescriptor();
    device_desc->setUSBVersion(USB_BCD);
    device_desc->setDeviceClass(TUSB_CLASS_MISC);
    device_desc->setDeviceSubClass(MISC_SUBCLASS_COMMON);
    device_desc->setDeviceProtocol(MISC_PROTOCOL_IAD);
    device_desc->setMaxPacketSize0(CFG_TUD_ENDPOINT0_SIZE);
    device_desc->setVendorID(USB_VID);
    device_desc->setProductID(USB_PID);
    device_desc->setDeviceVersion(0x0100);
    device_desc->setManufacturerStringIndex(0x01);
    device_desc->setProductStringIndex(0x02);
    device_desc->setSerialStringIndex(0x03);
}

static void build_config_descriptor()
{

    config_desc = new USB::ConfigurationDescriptor();
    config_desc->setMaxPower(100);
    config_desc->setName("BlueSCSI USB");

    device_desc->addChildDescriptor(config_desc);
}

static void build_msc_interface(USB::ConfigurationDescriptor *cfg_desc)
{
    //   Interface
    //   9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, _stridx,
    USB::InterfaceDescriptor *id_msc = new USB::InterfaceDescriptor();
    id_msc->setInterfaceClass(TUSB_CLASS_MSC);
    id_msc->setInterfaceSubClass(MSC_SUBCLASS_SCSI);
    id_msc->setInterfaceProtocol(MSC_PROTOCOL_BOT);
    id_msc->setName("TinyUSB MSC");
    cfg_desc->addChildDescriptor(id_msc);

    //   Endpoint Out
    //   7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,
    USB::EndpointDescriptor *ed_msc_out = new USB::EndpointDescriptor(TUSB_DIR_OUT);
    ed_msc_out->setAttributes(TUSB_XFER_BULK);
    ed_msc_out->setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_msc_out->setInterval(0);
    id_msc->addChildDescriptor(ed_msc_out);

    //   Endpoint In
    //   7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0
    USB::EndpointDescriptor *ed_msc_in = new USB::EndpointDescriptor(TUSB_DIR_IN);
    ed_msc_in->setAttributes(TUSB_XFER_BULK);
    ed_msc_in->setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_msc_in->setInterval(0);
    id_msc->addChildDescriptor(ed_msc_in);
}

static void build_cdc_interface(USB::ConfigurationDescriptor *cfg_desc)
{

    //   Interface Associate
    //   8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0,
    USB::InterfaceAssociationDescriptor *iface_associate = new USB::InterfaceAssociationDescriptor();
    iface_associate->setFunctionClass(TUSB_CLASS_CDC);
    iface_associate->setFunctionSubClass(CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL);
    iface_associate->setFunctionProtocol(CDC_COMM_PROTOCOL_NONE);
    cfg_desc->addChildDescriptor(iface_associate);

    //   CDC Control Interface
    //   9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, _stridx,
    USB::InterfaceDescriptor *iface_control = new USB::InterfaceDescriptor();
    iface_control->setAlternateSetting(0);
    iface_control->setInterfaceClass(TUSB_CLASS_CDC);
    iface_control->setInterfaceSubClass(CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL);
    iface_control->setInterfaceProtocol(CDC_COMM_PROTOCOL_NONE);
    iface_control->setName("TinyUSB CDC");
    iface_associate->addChildDescriptor(iface_control);

    // ** Needs to be created early so we can associate its iface number below.
    //   CDC Data Interface
    //   9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,
    USB::InterfaceDescriptor *iface_data = new USB::InterfaceDescriptor();
    iface_data->setAlternateSetting(0);
    iface_data->setInterfaceClass(TUSB_CLASS_CDC_DATA);
    iface_data->setInterfaceSubClass(0);
    iface_data->setInterfaceProtocol(0);
    iface_associate->addChildDescriptor(iface_data);

    //   CDC Header
    //   5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),
    USB::CDC::HeaderFunctionalDescriptor *header = new USB::CDC::HeaderFunctionalDescriptor();
    header->setBcdCdc(0x0120);
    iface_control->addChildDescriptor(header);

    //   CDC Call
    //   5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1),
    USB::CDC::CallManagementFunctionalDescriptor *call = new USB::CDC::CallManagementFunctionalDescriptor();
    call->setHandleCall(false);
    call->setSendRecvCall(false);
    call->setDataInterface(iface_data->getInterfaceNumber());
    iface_control->addChildDescriptor(call);

    //   CDC ACM: support line request + send break
    //   4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 6,
    USB::CDC::AbstractControlManagementFunctionalDescriptor *acm = new USB::CDC::AbstractControlManagementFunctionalDescriptor();
    acm->setSupportCommRequest(false);
    acm->setSupportLineRequest(true);
    acm->setSupportSendBreak(true);
    acm->setSupportNotificationNetworkConnection(false);
    iface_control->addChildDescriptor(acm);

    //   CDC Union
    //   5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),
    USB::CDC::UnionFunctionalDescriptor *union_desc = new USB::CDC::UnionFunctionalDescriptor();
    union_desc->setControlInterface(iface_control->getInterfaceNumber());
    union_desc->setSubordinateInterface(iface_data->getInterfaceNumber());
    iface_control->addChildDescriptor(union_desc);

    //   Endpoint Notification
    //   7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 16
    USB::EndpointDescriptor *ep_notif = new USB::EndpointDescriptor(TUSB_DIR_IN);
    ep_notif->setTransferType(TUSB_XFER_INTERRUPT);
    ep_notif->setMaxPacketSize(8);
    ep_notif->setInterval(16);
    iface_control->addChildDescriptor(ep_notif);

    // The example TUSB descriptor reserves the corresponding out EP number. Not sure if this
    // is necesary, but we'll do it...
    USB::EndpointDescriptor::reserveEndpointNumber(ep_notif->getEndpointAddress() & ~TUSB_DIR_IN_MASK);

    //   Endpoint Out
    //   7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,
    USB::EndpointDescriptor *ep_out = new USB::EndpointDescriptor(TUSB_DIR_OUT);
    ep_out->setTransferType(TUSB_XFER_BULK);
    ep_out->setMaxPacketSize(64);
    ep_out->setInterval(0);
    iface_data->addChildDescriptor(ep_out);

    //   Endpoint In
    //   7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0
    USB::EndpointDescriptor *ep_in = new USB::EndpointDescriptor(TUSB_DIR_IN);
    ep_in->setTransferType(TUSB_XFER_BULK);
    ep_in->setMaxPacketSize(64);
    ep_in->setInterval(0);
    iface_data->addChildDescriptor(ep_in);
}

void usb_descriptors_init()
{
    build_device_descriptor();
    build_config_descriptor();

    // // Create default devices
    // if (!g_disable_usb_cdc)
    // {
        build_cdc_interface(config_desc);
        build_cdc_interface(config_desc);
    // }
    // if (g_scsi_msc_mode)
    // {
        build_msc_interface(config_desc);
    // }
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
    static std::shared_ptr<std::vector<uint8_t>> saved_device_desc = nullptr;
    if (device_desc != nullptr)
    {
        saved_device_desc->clear();
    }
    saved_device_desc = std::make_shared<std::vector<uint8_t>>(device_desc->generateDescriptorBlock());
    return saved_device_desc->data();
}

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
extern "C" uint8_t const *tud_descriptor_device_qualifier_cb(void)
{
    // Store the last descriptor block so the memory isn't free-ed
    static std::shared_ptr<std::vector<uint8_t>> device_qualifier_desc = nullptr;
    if (device_qualifier_desc != nullptr)
    {
        device_qualifier_desc->clear();
    }
    device_qualifier_desc = std::make_shared<std::vector<uint8_t>>(device_desc->generateDeviceQualifierBlock());

    return device_qualifier_desc->data();
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index; // for multiple configurations

    // Store the last descriptor block so the memory isn't free-ed
    // TODO: Not sure if this is needed.... need to thing through this....
    static std::shared_ptr<std::vector<uint8_t>> config_qualifier_desc = nullptr;
    if (config_qualifier_desc != nullptr)
    {
        config_qualifier_desc->clear();
    }
    config_qualifier_desc = std::make_shared<std::vector<uint8_t>>(config_desc->generateDescriptorBlock());

    return config_qualifier_desc->data();
    // TODO: handle different speeds. For now, just assume Full Speed (USB 1.1), 12Mbps
    // #if TUD_OPT_HIGH_SPEED
    //   // Although we are highspeed, host may be fullspeed.
    //   return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
    // #else
    //   return desc_fs_configuration;
    // #endif
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static std::vector<uint8_t> usb_string_desc;
    usb_string_desc = USB::g_usb_string_descriptor.generateDescriptorBlock(index);
    return (uint16_t *)usb_string_desc.data();
}
