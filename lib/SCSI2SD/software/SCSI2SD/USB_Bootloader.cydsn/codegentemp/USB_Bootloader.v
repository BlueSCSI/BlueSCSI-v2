// ======================================================================
// USB_Bootloader.v generated from TopDesign.cysch
// 10/26/2013 at 18:55
// This file is auto generated. ANY EDITS YOU MAKE MAY BE LOST WHEN THIS FILE IS REGENERATED!!!
// ======================================================================

/* -- WARNING: The following section of defines are deprecated and will be removed in a future release -- */
`define CYDEV_CHIP_DIE_LEOPARD 1
`define CYDEV_CHIP_REV_LEOPARD_PRODUCTION 3
`define CYDEV_CHIP_REV_LEOPARD_ES3 3
`define CYDEV_CHIP_REV_LEOPARD_ES2 1
`define CYDEV_CHIP_REV_LEOPARD_ES1 0
`define CYDEV_CHIP_DIE_PSOC4A 2
`define CYDEV_CHIP_REV_PSOC4A_PRODUCTION 17
`define CYDEV_CHIP_REV_PSOC4A_ES0 17
`define CYDEV_CHIP_DIE_PANTHER 3
`define CYDEV_CHIP_REV_PANTHER_PRODUCTION 1
`define CYDEV_CHIP_REV_PANTHER_ES1 1
`define CYDEV_CHIP_REV_PANTHER_ES0 0
`define CYDEV_CHIP_DIE_PSOC5LP 4
`define CYDEV_CHIP_REV_PSOC5LP_PRODUCTION 0
`define CYDEV_CHIP_REV_PSOC5LP_ES0 0
`define CYDEV_CHIP_DIE_EXPECT 4
`define CYDEV_CHIP_REV_EXPECT 0
`define CYDEV_CHIP_DIE_ACTUAL 4
/* -- WARNING: The previous section of defines are deprecated and will be removed in a future release -- */
`define CYDEV_CHIP_FAMILY_UNKNOWN 0
`define CYDEV_CHIP_MEMBER_UNKNOWN 0
`define CYDEV_CHIP_FAMILY_PSOC3 1
`define CYDEV_CHIP_MEMBER_3A 1
`define CYDEV_CHIP_REVISION_3A_PRODUCTION 3
`define CYDEV_CHIP_REVISION_3A_ES3 3
`define CYDEV_CHIP_REVISION_3A_ES2 1
`define CYDEV_CHIP_REVISION_3A_ES1 0
`define CYDEV_CHIP_FAMILY_PSOC4 2
`define CYDEV_CHIP_MEMBER_4A 2
`define CYDEV_CHIP_REVISION_4A_PRODUCTION 17
`define CYDEV_CHIP_REVISION_4A_ES0 17
`define CYDEV_CHIP_FAMILY_PSOC5 3
`define CYDEV_CHIP_MEMBER_5A 3
`define CYDEV_CHIP_REVISION_5A_PRODUCTION 1
`define CYDEV_CHIP_REVISION_5A_ES1 1
`define CYDEV_CHIP_REVISION_5A_ES0 0
`define CYDEV_CHIP_MEMBER_5B 4
`define CYDEV_CHIP_REVISION_5B_PRODUCTION 0
`define CYDEV_CHIP_REVISION_5B_ES0 0
`define CYDEV_CHIP_FAMILY_USED 3
`define CYDEV_CHIP_MEMBER_USED 4
`define CYDEV_CHIP_REVISION_USED 0
// USBFS_v2_60(AudioDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="Audio">       <Nodes />     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, CDCDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="CDC">       <Nodes />     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, DeviceDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="Device">       <Nodes>         <DescriptorNode Key="USBDescriptor77">           <Value d6p1:type="DeviceDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>DEVICE</bDescriptorType>             <bLength>18</bLength>             <iwManufacturer>75</iwManufacturer>             <iwProduct>76</iwProduct>             <sManufacturer>Cypress Semiconductor</sManufacturer>             <sProduct>PSoC3 Bootloader</sProduct>             <sSerialNumber>0001</sSerialNumber>             <bDeviceClass>0</bDeviceClass>             <bDeviceSubClass>0</bDeviceSubClass>             <bDeviceProtocol>0</bDeviceProtocol>             <bMaxPacketSize0>0</bMaxPacketSize0>             <idVendor>1204</idVendor>             <idProduct>46877</idProduct>             <bcdDevice>12289</bcdDevice>             <iManufacturer>1</iManufacturer>             <iProduct>2</iProduct>             <iSerialNumber>0</iSerialNumber>             <bNumConfigurations>1</bNumConfigurations>             <bMemoryMgmt>0</bMemoryMgmt>             <bMemoryAlloc>0</bMemoryAlloc>           </Value>           <Nodes>             <DescriptorNode Key="USBDescriptor82">               <Value d8p1:type="ConfigDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>CONFIGURATION</bDescriptorType>                 <bLength>9</bLength>                 <iwConfiguration>0</iwConfiguration>                 <wTotalLength>41</wTotalLength>                 <bNumInterfaces>1</bNumInterfaces>                 <bConfigurationValue>0</bConfigurationValue>                 <iConfiguration>0</iConfiguration>                 <bmAttributes>128</bmAttributes>                 <bMaxPower>0</bMaxPower>               </Value>               <Nodes>                 <DescriptorNode Key="Interface86">                   <Value d10p1:type="InterfaceGeneralDescriptor" xmlns:d10p1="http://www.w3.org/2001/XMLSchema-instance">                     <bDescriptorType>ALTERNATE</bDescriptorType>                     <bLength>0</bLength>                     <DisplayName />                   </Value>                   <Nodes>                     <DescriptorNode Key="USBDescriptor87">                       <Value d12p1:type="InterfaceDescriptor" xmlns:d12p1="http://www.w3.org/2001/XMLSchema-instance">                         <bDescriptorType>INTERFACE</bDescriptorType>                         <bLength>9</bLength>                         <iwInterface>76</iwInterface>                         <bInterfaceClass>3</bInterfaceClass>                         <bNumEndpoints>2</bNumEndpoints>                         <bInterfaceSubClass>0</bInterfaceSubClass>                         <bInterfaceProtocol>0</bInterfaceProtocol>                         <iInterface>2</iInterface>                         <sInterface>PSoC3 Bootloader</sInterface>                       </Value>                       <Nodes>                         <DescriptorNode Key="USBDescriptor89">                           <Value d14p1:type="EndpointDescriptor" xmlns:d14p1="http://www.w3.org/2001/XMLSchema-instance">                             <bDescriptorType>ENDPOINT</bDescriptorType>                             <bLength>7</bLength>                             <DoubleBuffer>false</DoubleBuffer>                             <bInterval>1</bInterval>                             <bEndpointAddress>1</bEndpointAddress>                             <bmAttributes>3</bmAttributes>                             <wMaxPacketSize>64</wMaxPacketSize>                           </Value>                           <Nodes />                         </DescriptorNode>                         <DescriptorNode Key="USBDescriptor90">                           <Value d14p1:type="HIDDescriptor" xmlns:d14p1="http://www.w3.org/2001/XMLSchema-instance">                             <bDescriptorType>HID</bDescriptorType>                             <bLength>9</bLength>                             <bReportIndex>1</bReportIndex>                             <wReportIndex>55</wReportIndex>                             <bcdHID>0</bcdHID>                             <bCountryCode>0</bCountryCode>                             <bNumDescriptors>1</bNumDescriptors>                             <bDescriptorType1>34</bDescriptorType1>                             <wDescriptorLength>36</wDescriptorLength>                           </Value>                           <Nodes />                         </DescriptorNode>                         <DescriptorNode Key="USBDescriptor91">                           <Value d14p1:type="EndpointDescriptor" xmlns:d14p1="http://www.w3.org/2001/XMLSchema-instance">                             <bDescriptorType>ENDPOINT</bDescriptorType>                             <bLength>7</bLength>                             <DoubleBuffer>false</DoubleBuffer>                             <bInterval>1</bInterval>                             <bEndpointAddress>130</bEndpointAddress>                             <bmAttributes>3</bmAttributes>                             <wMaxPacketSize>64</wMaxPacketSize>                           </Value>                           <Nodes />                         </DescriptorNode>                       </Nodes>                     </DescriptorNode>                   </Nodes>                 </DescriptorNode>               </Nodes>             </DescriptorNode>           </Nodes>         </DescriptorNode>       </Nodes>     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, EnableCDCApi=true, EnableMidiApi=true, endpointMA=0, endpointMM=0, extern_cls=false, extern_vbus=false, extern_vnd=false, extJackCount=0, HIDReportDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="HIDReport">       <Nodes>         <DescriptorNode Key="USBDescriptor55">           <Value d6p1:type="HIDReportDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>HID_REPORT</bDescriptorType>             <bLength>2</bLength>             <Name>Generic HID</Name>             <wLength>36</wLength>           </Value>           <Nodes>             <DescriptorNode Key="USBDescriptor56">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="4" Type="USAGE_PAGE">                   <Description>(Generic Desktop Controls)</Description>                   <Value>                     <unsignedByte>5</unsignedByte>                     <unsignedByte>1</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor57">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="8" Type="USAGE">                   <Description>(Undefined)</Description>                   <Value>                     <unsignedByte>9</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor58">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="160" Type="COLLECTION">                   <Description>(Physical)</Description>                   <Value>                     <unsignedByte>161</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor59">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="8" Type="USAGE">                   <Description>(Undefined)</Description>                   <Value>                     <unsignedByte>9</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor60">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="160" Type="COLLECTION">                   <Description>(Physical)</Description>                   <Value>                     <unsignedByte>161</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor61">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="8" Type="USAGE">                   <Description>(Undefined)</Description>                   <Value>                     <unsignedByte>9</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor62">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="20" Type="LOGICAL_MINIMUM">                   <Value>                     <unsignedByte>21</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor63">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="36" Type="LOGICAL_MAXIMUM">                   <Value>                     <unsignedByte>37</unsignedByte>                     <unsignedByte>255</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor64">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="116" Type="REPORT_SIZE">                   <Value>                     <unsignedByte>117</unsignedByte>                     <unsignedByte>8</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor65">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="148" Type="REPORT_COUNT">                   <Value>                     <unsignedByte>149</unsignedByte>                     <unsignedByte>64</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor66">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="144" Type="OUTPUT">                   <Description>(Var)</Description>                   <Value>                     <unsignedByte>145</unsignedByte>                     <unsignedByte>2</unsignedByte>                   </Value>                   <Kind>Bits</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor67">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="8" Type="USAGE">                   <Description>(Undefined)</Description>                   <Value>                     <unsignedByte>9</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>List</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor68">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="20" Type="LOGICAL_MINIMUM">                   <Value>                     <unsignedByte>21</unsignedByte>                     <unsignedByte>0</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor69">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="36" Type="LOGICAL_MAXIMUM">                   <Value>                     <unsignedByte>37</unsignedByte>                     <unsignedByte>255</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor70">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="116" Type="REPORT_SIZE">                   <Value>                     <unsignedByte>117</unsignedByte>                     <unsignedByte>8</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor71">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="148" Type="REPORT_COUNT">                   <Value>                     <unsignedByte>149</unsignedByte>                     <unsignedByte>64</unsignedByte>                   </Value>                   <Kind>Int</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor72">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="128" Type="INPUT">                   <Description>(Var)</Description>                   <Value>                     <unsignedByte>129</unsignedByte>                     <unsignedByte>2</unsignedByte>                   </Value>                   <Kind>Bits</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor73">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="192" Type="END_COLLECTION">                   <Value>                     <unsignedByte>192</unsignedByte>                   </Value>                   <Kind>None</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>             <DescriptorNode Key="USBDescriptor74">               <Value d8p1:type="HIDReportItemDescriptor" xmlns:d8p1="http://www.w3.org/2001/XMLSchema-instance">                 <bDescriptorType>HID_REPORT_ITEM</bDescriptorType>                 <bLength>1</bLength>                 <Item Code="192" Type="END_COLLECTION">                   <Value>                     <unsignedByte>192</unsignedByte>                   </Value>                   <Kind>None</Kind>                 </Item>               </Value>               <Nodes />             </DescriptorNode>           </Nodes>         </DescriptorNode>       </Nodes>     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, max_interfaces_num=1, MidiDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="Midi">       <Nodes />     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, Mode=false, mon_vbus=false, out_sof=false, Pid=F232, rm_arb_int=false, rm_dma_1=true, rm_dma_2=true, rm_dma_3=true, rm_dma_4=true, rm_dma_5=true, rm_dma_6=true, rm_dma_7=true, rm_dma_8=true, rm_dp_int=false, rm_ep_isr_0=false, rm_ep_isr_1=false, rm_ep_isr_2=false, rm_ep_isr_3=true, rm_ep_isr_4=true, rm_ep_isr_5=true, rm_ep_isr_6=true, rm_ep_isr_7=true, rm_ep_isr_8=true, rm_ord_int=true, rm_sof_int=false, rm_usb_int=false, StringDescriptors=<?xml version="1.0" encoding="utf-16"?> <Tree xmlns:CustomizerVersion="2_60">   <Tree_x0020_Descriptors>     <DescriptorNode Key="String">       <Nodes>         <DescriptorNode Key="LANGID">           <Value d6p1:type="StringZeroDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>STRING</bDescriptorType>             <bLength>4</bLength>             <wLANGID>1033</wLANGID>           </Value>           <Nodes />         </DescriptorNode>         <DescriptorNode Key="USBDescriptor75">           <Value d6p1:type="StringDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>STRING</bDescriptorType>             <bLength>44</bLength>             <snType>USER_ENTERED_TEXT</snType>             <bString>Cypress Semiconductor</bString>             <bUsed>false</bUsed>           </Value>           <Nodes />         </DescriptorNode>         <DescriptorNode Key="USBDescriptor76">           <Value d6p1:type="StringDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>STRING</bDescriptorType>             <bLength>34</bLength>             <snType>USER_ENTERED_TEXT</snType>             <bString>PSoC3 Bootloader</bString>             <bUsed>false</bUsed>           </Value>           <Nodes />         </DescriptorNode>       </Nodes>     </DescriptorNode>     <DescriptorNode Key="SpecialString">       <Nodes>         <DescriptorNode Key="Serial">           <Value d6p1:type="StringDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>STRING</bDescriptorType>             <bLength>10</bLength>             <snType>USER_ENTERED_TEXT</snType>             <bString>0001</bString>             <bUsed>true</bUsed>           </Value>           <Nodes />         </DescriptorNode>         <DescriptorNode Key="EE">           <Value d6p1:type="StringDescriptor" xmlns:d6p1="http://www.w3.org/2001/XMLSchema-instance">             <bDescriptorType>STRING</bDescriptorType>             <bLength>16</bLength>             <snType>USER_ENTERED_TEXT</snType>             <bString>MSFT100</bString>             <bUsed>false</bUsed>           </Value>           <Nodes />         </DescriptorNode>       </Nodes>     </DescriptorNode>   </Tree_x0020_Descriptors> </Tree>, Vid=04B4, CY_COMPONENT_NAME=USBFS_v2_60, CY_CONTROL_FILE=<:default:>, CY_FITTER_NAME=USBFS, CY_INSTANCE_SHORT_NAME=USBFS, CY_MAJOR_VERSION=2, CY_MINOR_VERSION=60, CY_REMOVE=false, CY_SUPPRESS_API_GEN=false, CY_VERSION=cydsfit No Version Information Found, INSTANCE_NAME=USBFS, )
module USBFS_v2_60_0 (
    sof,
    vbusdet);
    output      sof;
    input       vbusdet;


          wire [7:0] dma_req;
          wire [8:0] ept_int;
          wire  Net_1106;
          wire [7:0] Net_1105;
          wire  Net_1104;
          wire  Net_1103;
          wire  Net_1102;
          wire  Net_1101;
          wire  Net_1100;
          wire  Net_1099;
          wire  Net_1098;
          wire  Net_1097;
          wire  Net_1096;
          wire  Net_1013;
          wire  Net_1014;
          wire  Net_1015;
          wire  Net_1016;
          wire  Net_1017;
          wire  Net_1018;
          wire  Net_1019;
          wire  Net_1020;
          wire  Net_1010;
    electrical  Net_1000;
          wire  Net_79;
          wire  Net_81;
          wire  Net_95;
    electrical  Net_597;
          wire  Net_824;

    cy_psoc3_usb_v1_0 USB (
        .dp(Net_1000),
        .dm(Net_597),
        .sof_int(sof),
        .arb_int(Net_79),
        .usb_int(Net_81),
        .ept_int(ept_int[8:0]),
        .ord_int(Net_95),
        .dma_req(dma_req[7:0]),
        .dma_termin(Net_824));


	cy_isr_v1_0
		#(.int_type(2'b10))
		sof_int
		 (.int_signal(sof));



	cy_isr_v1_0
		#(.int_type(2'b10))
		arb_int
		 (.int_signal(Net_79));



	cy_isr_v1_0
		#(.int_type(2'b10))
		bus_reset
		 (.int_signal(Net_81));



	cy_isr_v1_0
		#(.int_type(2'b10))
		ep_0
		 (.int_signal(ept_int[0:0]));



	cy_isr_v1_0
		#(.int_type(2'b10))
		ep_1
		 (.int_signal(ept_int[1:1]));



	cy_isr_v1_0
		#(.int_type(2'b10))
		ep_2
		 (.int_signal(ept_int[2:2]));


	wire [0:0] tmpOE__Dm_net;
	wire [0:0] tmpFB_0__Dm_net;
	wire [0:0] tmpIO_0__Dm_net;
	wire [0:0] tmpINTERRUPT_0__Dm_net;
	electrical [0:0] tmpSIOVREF__Dm_net;

	cy_psoc3_pins_v1_10
		#(.id("f9248435-5d3e-4e4d-bbae-bdae8795c3dd/8b77a6c4-10a0-4390-971c-672353e2a49c"),
		  .drive_mode(3'b000),
		  .ibuf_enabled(1'b0),
		  .init_dr_st(1'b0),
		  .input_clk_en(0),
		  .input_sync(1'b1),
		  .input_sync_mode(1'b0),
		  .intr_mode(2'b00),
		  .invert_in_clock(0),
		  .invert_in_clock_en(0),
		  .invert_in_reset(0),
		  .invert_out_clock(0),
		  .invert_out_clock_en(0),
		  .invert_out_reset(0),
		  .io_voltage(""),
		  .layout_mode("NONCONTIGUOUS"),
		  .oe_conn(1'b0),
		  .oe_reset(0),
		  .oe_sync(1'b0),
		  .output_clk_en(0),
		  .output_clock_mode(1'b0),
		  .output_conn(1'b0),
		  .output_mode(1'b0),
		  .output_reset(0),
		  .output_sync(1'b0),
		  .pa_in_clock(-1),
		  .pa_in_clock_en(-1),
		  .pa_in_reset(-1),
		  .pa_out_clock(-1),
		  .pa_out_clock_en(-1),
		  .pa_out_reset(-1),
		  .pin_aliases(""),
		  .pin_mode("A"),
		  .por_state(4),
		  .use_annotation(1'b0),
		  .sio_group_cnt(0),
		  .sio_hyst(1'b0),
		  .sio_ibuf(""),
		  .sio_info(2'b00),
		  .sio_obuf(""),
		  .sio_refsel(""),
		  .sio_vtrip(""),
		  .slew_rate(1'b0),
		  .spanning(1),
		  .vtrip(2'b10),
		  .width(1))
		Dm
		 (.oe(tmpOE__Dm_net),
		  .y({1'b0}),
		  .fb({tmpFB_0__Dm_net[0:0]}),
		  .analog({Net_597}),
		  .io({tmpIO_0__Dm_net[0:0]}),
		  .siovref(tmpSIOVREF__Dm_net),
		  .interrupt({tmpINTERRUPT_0__Dm_net[0:0]}),
		  .in_clock({1'b0}),
		  .in_clock_en({1'b1}),
		  .in_reset({1'b0}),
		  .out_clock({1'b0}),
		  .out_clock_en({1'b1}),
		  .out_reset({1'b0}));

	assign tmpOE__Dm_net = (`CYDEV_CHIP_MEMBER_USED == `CYDEV_CHIP_MEMBER_3A && `CYDEV_CHIP_REVISION_USED < `CYDEV_CHIP_REVISION_3A_ES3) ? ~{1'b1} : {1'b1};

	wire [0:0] tmpOE__Dp_net;
	wire [0:0] tmpFB_0__Dp_net;
	wire [0:0] tmpIO_0__Dp_net;
	electrical [0:0] tmpSIOVREF__Dp_net;

	cy_psoc3_pins_v1_10
		#(.id("f9248435-5d3e-4e4d-bbae-bdae8795c3dd/618a72fc-5ddd-4df5-958f-a3d55102db42"),
		  .drive_mode(3'b000),
		  .ibuf_enabled(1'b0),
		  .init_dr_st(1'b0),
		  .input_clk_en(0),
		  .input_sync(1'b1),
		  .input_sync_mode(1'b0),
		  .intr_mode(2'b10),
		  .invert_in_clock(0),
		  .invert_in_clock_en(0),
		  .invert_in_reset(0),
		  .invert_out_clock(0),
		  .invert_out_clock_en(0),
		  .invert_out_reset(0),
		  .io_voltage(""),
		  .layout_mode("CONTIGUOUS"),
		  .oe_conn(1'b0),
		  .oe_reset(0),
		  .oe_sync(1'b0),
		  .output_clk_en(0),
		  .output_clock_mode(1'b0),
		  .output_conn(1'b0),
		  .output_mode(1'b0),
		  .output_reset(0),
		  .output_sync(1'b0),
		  .pa_in_clock(-1),
		  .pa_in_clock_en(-1),
		  .pa_in_reset(-1),
		  .pa_out_clock(-1),
		  .pa_out_clock_en(-1),
		  .pa_out_reset(-1),
		  .pin_aliases(""),
		  .pin_mode("I"),
		  .por_state(4),
		  .use_annotation(1'b0),
		  .sio_group_cnt(0),
		  .sio_hyst(1'b0),
		  .sio_ibuf(""),
		  .sio_info(2'b00),
		  .sio_obuf(""),
		  .sio_refsel(""),
		  .sio_vtrip(""),
		  .slew_rate(1'b0),
		  .spanning(0),
		  .vtrip(2'b00),
		  .width(1))
		Dp
		 (.oe(tmpOE__Dp_net),
		  .y({1'b0}),
		  .fb({tmpFB_0__Dp_net[0:0]}),
		  .analog({Net_1000}),
		  .io({tmpIO_0__Dp_net[0:0]}),
		  .siovref(tmpSIOVREF__Dp_net),
		  .interrupt({Net_1010}),
		  .in_clock({1'b0}),
		  .in_clock_en({1'b1}),
		  .in_reset({1'b0}),
		  .out_clock({1'b0}),
		  .out_clock_en({1'b1}),
		  .out_reset({1'b0}));

	assign tmpOE__Dp_net = (`CYDEV_CHIP_MEMBER_USED == `CYDEV_CHIP_MEMBER_3A && `CYDEV_CHIP_REVISION_USED < `CYDEV_CHIP_REVISION_3A_ES3) ? ~{1'b1} : {1'b1};


	cy_isr_v1_0
		#(.int_type(2'b10))
		dp_int
		 (.int_signal(Net_1010));



	cy_clock_v1_0
		#(.id("f9248435-5d3e-4e4d-bbae-bdae8795c3dd/03f503a7-085a-4304-b786-de885b1c2f21"),
		  .source_clock_id("75C2148C-3656-4d8a-846D-0CAE99AB6FF7"),
		  .divisor(0),
		  .period("0"),
		  .is_direct(1),
		  .is_digital(1))
		Clock_vbus
		 (.clock_out(Net_1099));




endmodule

// top
module top ;

    electrical  Net_36;
    electrical  Net_35;
    electrical  Net_34;
    electrical  Net_33;
    electrical  Net_32;
    electrical  Net_31;
    electrical  Net_30;
    electrical  Net_29;
    electrical  Net_28;
    electrical  Net_27;
    electrical [7:0] Net_37;
          wire  Net_41;
          wire  Net_40;

    USBFS_v2_60_0 USBFS (
        .sof(Net_40),
        .vbusdet(1'b0));

	wire [7:0] tmpOE__SCSI_Out_DBx_net;
	wire [7:0] tmpFB_7__SCSI_Out_DBx_net;
	wire [7:0] tmpIO_7__SCSI_Out_DBx_net;
	wire [0:0] tmpINTERRUPT_0__SCSI_Out_DBx_net;
	electrical [0:0] tmpSIOVREF__SCSI_Out_DBx_net;

	cy_psoc3_pins_v1_10
		#(.id("52f31aa9-2f0a-497d-9a1f-1424095e13e6"),
		  .drive_mode(24'b110_110_110_110_110_110_110_110),
		  .ibuf_enabled(8'b1_1_1_1_1_1_1_1),
		  .init_dr_st(8'b0_0_0_0_0_0_0_0),
		  .input_clk_en(0),
		  .input_sync(8'b1_1_1_1_1_1_1_1),
		  .input_sync_mode(8'b0_0_0_0_0_0_0_0),
		  .intr_mode(16'b00_00_00_00_00_00_00_00),
		  .invert_in_clock(0),
		  .invert_in_clock_en(0),
		  .invert_in_reset(0),
		  .invert_out_clock(0),
		  .invert_out_clock_en(0),
		  .invert_out_reset(0),
		  .io_voltage(", , , , , , , 5"),
		  .layout_mode("NONCONTIGUOUS"),
		  .oe_conn(8'b0_0_0_0_0_0_0_0),
		  .oe_reset(0),
		  .oe_sync(8'b0_0_0_0_0_0_0_0),
		  .output_clk_en(0),
		  .output_clock_mode(8'b0_0_0_0_0_0_0_0),
		  .output_conn(8'b0_0_0_0_0_0_0_0),
		  .output_mode(8'b0_0_0_0_0_0_0_0),
		  .output_reset(0),
		  .output_sync(8'b0_0_0_0_0_0_0_0),
		  .pa_in_clock(-1),
		  .pa_in_clock_en(-1),
		  .pa_in_reset(-1),
		  .pa_out_clock(-1),
		  .pa_out_clock_en(-1),
		  .pa_out_reset(-1),
		  .pin_aliases("DB0,DB1,DB2,DB3,DB4,DB5,DB6,DB7"),
		  .pin_mode("OOOOOOOO"),
		  .por_state(4),
		  .use_annotation(8'b1_1_1_1_1_1_1_1),
		  .sio_group_cnt(0),
		  .sio_hyst(8'b0_0_0_0_0_0_0_0),
		  .sio_ibuf(""),
		  .sio_info(16'b00_00_00_00_00_00_00_00),
		  .sio_obuf(""),
		  .sio_refsel(""),
		  .sio_vtrip(""),
		  .slew_rate(8'b0_0_0_0_0_0_0_0),
		  .spanning(1),
		  .vtrip(16'b10_10_10_10_10_10_10_10),
		  .width(8))
		SCSI_Out_DBx
		 (.oe(tmpOE__SCSI_Out_DBx_net),
		  .y({8'b0}),
		  .fb({tmpFB_7__SCSI_Out_DBx_net[7:0]}),
		  .io({tmpIO_7__SCSI_Out_DBx_net[7:0]}),
		  .siovref(tmpSIOVREF__SCSI_Out_DBx_net),
		  .interrupt({tmpINTERRUPT_0__SCSI_Out_DBx_net[0:0]}),
		  .annotation({Net_37[7:0]}),
		  .in_clock({1'b0}),
		  .in_clock_en({1'b1}),
		  .in_reset({1'b0}),
		  .out_clock({1'b0}),
		  .out_clock_en({1'b1}),
		  .out_reset({1'b0}));

	assign tmpOE__SCSI_Out_DBx_net = (`CYDEV_CHIP_MEMBER_USED == `CYDEV_CHIP_MEMBER_3A && `CYDEV_CHIP_REVISION_USED < `CYDEV_CHIP_REVISION_3A_ES3) ? ~{8'b11111111} : {8'b11111111};

	wire [9:0] tmpOE__SCSI_Out_net;
	wire [9:0] tmpFB_9__SCSI_Out_net;
	wire [9:0] tmpIO_9__SCSI_Out_net;
	wire [0:0] tmpINTERRUPT_0__SCSI_Out_net;
	electrical [0:0] tmpSIOVREF__SCSI_Out_net;

	cy_psoc3_pins_v1_10
		#(.id("11f071e8-9c92-47e0-872a-3f48765a75b8"),
		  .drive_mode(30'b110_110_110_110_110_110_110_110_110_110),
		  .ibuf_enabled(10'b1_1_1_1_1_1_1_1_1_1),
		  .init_dr_st(10'b0_0_0_0_0_0_0_0_0_0),
		  .input_clk_en(0),
		  .input_sync(10'b1_1_1_1_1_1_1_1_1_1),
		  .input_sync_mode(10'b0_0_0_0_0_0_0_0_0_0),
		  .intr_mode(20'b00_00_00_00_00_00_00_00_00_00),
		  .invert_in_clock(0),
		  .invert_in_clock_en(0),
		  .invert_in_reset(0),
		  .invert_out_clock(0),
		  .invert_out_clock_en(0),
		  .invert_out_reset(0),
		  .io_voltage("5, 5, 5, 5, 5, 5, 5, 5, 5, 5"),
		  .layout_mode("NONCONTIGUOUS"),
		  .oe_conn(10'b0_0_0_0_0_0_0_0_0_0),
		  .oe_reset(0),
		  .oe_sync(10'b0_0_0_0_0_0_0_0_0_0),
		  .output_clk_en(0),
		  .output_clock_mode(10'b0_0_0_0_0_0_0_0_0_0),
		  .output_conn(10'b0_0_0_0_0_0_0_0_0_0),
		  .output_mode(10'b0_0_0_0_0_0_0_0_0_0),
		  .output_reset(0),
		  .output_sync(10'b0_0_0_0_0_0_0_0_0_0),
		  .pa_in_clock(-1),
		  .pa_in_clock_en(-1),
		  .pa_in_reset(-1),
		  .pa_out_clock(-1),
		  .pa_out_clock_en(-1),
		  .pa_out_reset(-1),
		  .pin_aliases("DBP_raw,ATN,BSY,ACK,RST,MSG,SEL,CD,REQ,IO_raw"),
		  .pin_mode("OOOOOOOOOO"),
		  .por_state(4),
		  .use_annotation(10'b1_1_1_1_1_1_1_1_1_1),
		  .sio_group_cnt(0),
		  .sio_hyst(10'b0_0_0_0_0_0_0_0_0_0),
		  .sio_ibuf(""),
		  .sio_info(20'b00_00_00_00_00_00_00_00_00_00),
		  .sio_obuf(""),
		  .sio_refsel(""),
		  .sio_vtrip(""),
		  .slew_rate(10'b0_0_0_0_0_0_0_0_0_0),
		  .spanning(1),
		  .vtrip(20'b10_10_10_10_10_10_10_10_10_10),
		  .width(10))
		SCSI_Out
		 (.oe(tmpOE__SCSI_Out_net),
		  .y({10'b0}),
		  .fb({tmpFB_9__SCSI_Out_net[9:0]}),
		  .io({tmpIO_9__SCSI_Out_net[9:0]}),
		  .siovref(tmpSIOVREF__SCSI_Out_net),
		  .interrupt({tmpINTERRUPT_0__SCSI_Out_net[0:0]}),
		  .annotation({Net_36, Net_35, Net_34, Net_33, Net_32, Net_31, Net_30, Net_29, Net_28, Net_27}),
		  .in_clock({1'b0}),
		  .in_clock_en({1'b1}),
		  .in_reset({1'b0}),
		  .out_clock({1'b0}),
		  .out_clock_en({1'b1}),
		  .out_reset({1'b0}));

	assign tmpOE__SCSI_Out_net = (`CYDEV_CHIP_MEMBER_USED == `CYDEV_CHIP_MEMBER_3A && `CYDEV_CHIP_REVISION_USED < `CYDEV_CHIP_REVISION_3A_ES3) ? ~{10'b1111111111} : {10'b1111111111};



endmodule

