#include "usb/usb_descriptors.h"
#define USB_VID 0xCafe
#define USB_BCD 0x0200
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                 _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))


void save_descriptor(const char *name, const uint8_t *buf, size_t len);

static USB::DeviceDescriptor dd;
static USB::ConfigurationDescriptor cd_config;
static USB::ConfigurationDescriptor cd_cdc0;
static USB::ConfigurationDescriptor cd_cdc1;
static USB::ConfigurationDescriptor cd_msc;




void build_device_descriptor(){
    dd.setUSBVersion(USB_BCD);
    dd.setDeviceClass(TUSB_CLASS_MISC);
    dd.setDeviceSubClass(MISC_SUBCLASS_COMMON);
    dd.setDeviceProtocol(MISC_PROTOCOL_IAD);
    dd.setMaxPacketSize0(CFG_TUD_ENDPOINT0_SIZE);
    dd.setVendorID(USB_VID);
    dd.setProductID(USB_PID);
    dd.setDeviceVersion(0x0100);
    dd.setManufacturerStringIndex(0x01);
    dd.setProductStringIndex(0x02);
    dd.setSerialStringIndex(0x03);

}

void check_configuration_descriptors(){

    USB::ConfigurationDescriptor cd_config;
    // cd_config.


    // void setConfigurationValue(uint8_t value) { desc_.bConfigurationValue = value; }
    // void setStringIndex(uint8_t index) { desc_.iConfiguration = index; }
    // void setAttributes(uint8_t attributes) { desc_.bmAttributes = attributes; }
    // void setMaxPower(uint8_t maxPower) { desc_.bMaxPower = maxPower; }

    // bool addInterface(const InterfaceDescriptor& interface);

        // // Config number, interface count, string index, total length, attribute, power in mA
        // TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

        // // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
        // TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

        // // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
        // TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),

        // // Interface number, string index, EP Out & EP In address, EP size
        // TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),


}


void build_msc_interface(){
// // Length of template descriptor: 23 bytes
// #define TUD_MSC_DESC_LEN    (9 + 7 + 7)

// // Interface number, string index, EP Out & EP In address, EP size
// #define TUD_MSC_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
//   /* Interface */\
//   9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_MSC, MSC_SUBCLASS_SCSI, MSC_PROTOCOL_BOT, _stridx,\
//   /* Endpoint Out */\
//   7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
//   /* Endpoint In */\
//   7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0
        // // Interface number, string index, EP Out & EP In address, EP size
        // TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),


    USB::InterfaceDescriptor id_msc;
    // id_msc.setInterfaceNumber(USB::ITF_NUM_MSC);
    id_msc.setInterfaceClass(TUSB_CLASS_MSC);
    id_msc.setInterfaceSubClass(MSC_SUBCLASS_SCSI);
    id_msc.setInterfaceProtocol(MSC_PROTOCOL_BOT);
    id_msc.setName("TinyUSB MSC");
    // id_msc.setStringIndex(USB::STR_IDX_MSC);

    USB::EndpointDescriptor ed_msc_out;
    // ed_msc_out.setEndpointAddress(USB::EPNUM_MSC_OUT);
    ed_msc_out.setAttributes(TUSB_XFER_BULK);
    ed_msc_out.setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_msc_out.setInterval(0);
    ed_msc_out.setDirection(TUSB_DIR_OUT);
    id_msc.addEndpoint(ed_msc_out);

    USB::EndpointDescriptor ed_msc_in;
    // ed_msc_in.setEndpointAddress(USB::EPNUM_MSC_IN);
    ed_msc_in.setAttributes(TUSB_XFER_BULK);
    ed_msc_in.setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_msc_in.setInterval(0);
    ed_msc_in.setDirection(TUSB_DIR_IN);
    id_msc.addEndpoint(ed_msc_in);

    cd_msc.addInterface(id_msc);
}


void_build_cdc_interface(){

// https://www.keil.com/pack/doc/mw6/USB/html/_c_d_c.html
//          Standard Device Descriptor
//          Standard Configuration Descriptor
//          Interface Association Descriptor
//          CDC Header Functional Descriptor
//          CDC Union Functional Descriptor
//          Call Management Functional Descriptor
//          Abstract Control Management Functional Descriptor
//          Standard Interface Descriptor for the CDC Class communication interface
//          Standard Endpoint Descriptor for Interrupt IN endpoint
//          Standard Interface Descriptor for the CDC Class data interface
//          Standard Endpoint Descriptors for Bulk IN and Bulk OUT endpoints

//     // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
// TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 4, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),


// CDC Descriptor Template
// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#define TUD_CDC_DESCRIPTOR(_itfnum, _stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize) \
  /* Interface Associate */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0,\
  /* CDC Control Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, _stridx,\
  /* CDC Header */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),\
  /* CDC Call */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1),\
  /* CDC ACM: support line request + send break */\
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 6,\
  /* CDC Union */\
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1),\
  /* Endpoint Notification */\
  7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 16,\
  /* CDC Data Interface */\
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0


    USB::InterfaceDescriptor id_cdc;
    id_cdc.setInterfaceClass(TUSB_CLASS_CDC);
    id_cdc.setInterfaceSubClass(CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL);
    id_cdc.setInterfaceProtocol(CDC_COMM_PROTOCOL_NONE);
    id_cdc.setName("TinyUSB CDC");
    // id_cdc.setStringIndex(USB::STR_IDX_CDC);

    USB::EndpointDescriptor ed_cdc_notif;
    // ed_cdc_notif.setEndpointAddress(USB::EPNUM_CDC_0_NOTIF);
    ed_cdc_notif.setAttributes(TUSB_XFER_INTERRUPT);
    ed_cdc_notif.setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_cdc_notif.setInterval(0);
    ed_cdc_notif.setDirection(TUSB_DIR_IN);
    id_cdc.addEndpoint(ed_cdc_notif);

    USB::EndpointDescriptor ed_cdc_out;
    ed_cdc_out.setEndpointAddress(USB::EPNUM_CDC_0_OUT);
    ed_cdc_out.setAttributes(TUSB_XFER_BULK);
    ed_cdc_out.setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_cdc_out.setInterval(0);
    ed_cdc_out.setDirection(TUSB_DIR_OUT);
    id_cdc.addEndpoint(ed_cdc_out);

    USB::EndpointDescriptor ed_cdc_in;
    // ed_cdc_in.setEndpointAddress(USB::EPNUM_CDC_0_IN);
    ed_cdc_in.setAttributes(TUSB_XFER_BULK);
    ed_cdc_in.setMaxPacketSize(TUSB_EPSIZE_BULK_FS);
    ed_cdc_in.setInterval(0);
    ed_cdc_in.setDirection(TUSB_DIR_IN);
    id_cdc.addEndpoint(ed_cdc_in);

    cd_cdc0.addInterface(id_cdc);
}

extern "C" void run_usb_desc_tests(){
    build_device_descriptor();

        save_descriptor("custom_device_descriptor", dd.getDescriptorBlock(), dd.getDescriptorBlockLength());

    return;

}