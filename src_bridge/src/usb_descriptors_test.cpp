#include "usb/usb_descriptors.h"
#define USB_VID 0xCafe
#define USB_BCD 0x0200
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                 _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))


void save_descriptor(const char *name, const uint8_t *buf, size_t len);

void build_cdc_interface(USB::ConfigurationDescriptor *cfg_desc);
void build_msc_interface(USB::ConfigurationDescriptor *cfg_desc);

static USB::DeviceDescriptor *dd;

std::string get_desc_block_type(tusb_desc_type_t mytype){
    switch (mytype)
    {
    case TUSB_DESC_DEVICE: return "TUSB_DESC_DEVICE";
    case TUSB_DESC_CONFIGURATION: return "TUSB_DESC_CONFIGURATION";
    case TUSB_DESC_STRING: return "TUSB_DESC_STRING";
    case TUSB_DESC_INTERFACE: return "TUSB_DESC_INTERFACE";
    case TUSB_DESC_ENDPOINT: return "TUSB_DESC_ENDPOINT";
    case TUSB_DESC_DEVICE_QUALIFIER: return "TUSB_DESC_DEVICE_QUALIFIER";
    case TUSB_DESC_OTHER_SPEED_CONFIG: return "TUSB_DESC_OTHER_SPEED_CONFIG";
    case TUSB_DESC_INTERFACE_POWER: return "TUSB_DESC_INTERFACE_POWER";
    case TUSB_DESC_OTG: return "TUSB_DESC_OTG";
    case TUSB_DESC_DEBUG: return "TUSB_DESC_DEBUG";
    case TUSB_DESC_INTERFACE_ASSOCIATION: return "TUSB_DESC_INTERFACE_ASSOCIATION";
    case TUSB_DESC_BOS: return "TUSB_DESC_BOS";
    case TUSB_DESC_DEVICE_CAPABILITY: return "TUSB_DESC_DEVICE_CAPABILITY";
    case TUSB_DESC_FUNCTIONAL: return "TUSB_DESC_FUNCTIONAL";
    // case TUSB_DESC_CS_DEVICE: return "TUSB_DESC_CS_DEVICE";
    case TUSB_DESC_CS_CONFIGURATION: return "TUSB_DESC_CS_CONFIGURATION";
    case TUSB_DESC_CS_STRING: return "TUSB_DESC_CS_STRING";
    case TUSB_DESC_CS_INTERFACE: return "TUSB_DESC_CS_INTERFACE";
    case TUSB_DESC_CS_ENDPOINT: return "TUSB_DESC_CS_ENDPOINT";
    case TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION: return "TUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION";
    case TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION: return "TUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION";
    default:
        return "UNKNOWN";
    }
}





void build_device_descriptor(){
    dd = new USB::DeviceDescriptor();
    dd->setUSBVersion(USB_BCD);
    dd->setDeviceClass(TUSB_CLASS_MISC);
    dd->setDeviceSubClass(MISC_SUBCLASS_COMMON);
    dd->setDeviceProtocol(MISC_PROTOCOL_IAD);
    dd->setMaxPacketSize0(CFG_TUD_ENDPOINT0_SIZE);
    dd->setVendorID(USB_VID);
    dd->setProductID(USB_PID);
    dd->setDeviceVersion(0x0100);
    dd->setManufacturerStringIndex(0x01);
    dd->setProductStringIndex(0x02);
    dd->setSerialStringIndex(0x03);

    std::vector<uint8_t> descriptor_block = dd->generateDescriptorBlock();
    save_descriptor("generated_device_descriptor", descriptor_block.data(), descriptor_block.size());
}

USB::ConfigurationDescriptor * check_configuration_descriptors(){

    USB::ConfigurationDescriptor *cd_config = new USB::ConfigurationDescriptor();
    cd_config->setMaxPower(100);

    build_cdc_interface(cd_config);
    build_cdc_interface(cd_config);
    build_msc_interface(cd_config);

    dd->addChildDescriptor(cd_config);

    return cd_config;
        // // Config number, interface count, string index, total length, attribute, power in mA
        // TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

        // // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
        // TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

        // // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
        // TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),

        // // Interface number, string index, EP Out & EP In address, EP size
        // TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),


}


void build_msc_interface(USB::ConfigurationDescriptor *cfg_desc){
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


void build_cdc_interface(USB::ConfigurationDescriptor *cfg_desc){

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

extern "C" void run_usb_desc_tests(){
    build_device_descriptor();
    USB::ConfigurationDescriptor *config_desc = check_configuration_descriptors();

    char filename[128] = "";

    std::vector<uint8_t> temp_block = dd->generateDescriptorBlock();
    save_descriptor("generated_device_descriptor", temp_block.data(), temp_block.size());
    temp_block = config_desc->generateDescriptorBlock();
    save_descriptor("generated_config_descriptor", temp_block.data(), temp_block.size());

    for (int i = 0; i < 6; i++)
    {
        std::shared_ptr<USB::StringDescriptor::tusb_desc_string_fixed_len_t> new_desc = USB::g_usb_string_descriptor.generateDescriptor(i);
        log("generated_string_desc_arr[", i, "]: ", USB::g_usb_string_descriptor.getString(i).c_str());
        snprintf(filename, sizeof(filename), "generated_string_desc_arr-%d", i);
        save_descriptor(filename, (uint8_t *)new_desc.get(), new_desc->bLength);
    }
    // save_descriptor("new_config_descriptor", (uint8_t*)config_desc->getDescriptorBlock(), config_desc->getDescriptorBlockLength());
  
    // save_descriptor("custom_string_descriptor_")

    return;

}