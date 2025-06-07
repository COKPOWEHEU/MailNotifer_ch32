#define UART_DECLARATIONS 2
#include "uart.h"

#include "usbd_lib.h"
#include "hardware.h"
#define USB_CLASS_HID_IMPLEMENTATION
#include "usb_class_hid.h"
#include "tty.h"
#include "usb_class_virfat.h"

#define STD_DESCR_LANG	0
#define STD_DESCR_VEND	1
#define STD_DESCR_PROD	2
#define STD_DESCR_SN	3
#define STR_TTY		4

USB_STRING(USB_StringLangDescriptor, u"\x0409"); //lang US
USB_STRING(USB_StringManufacturingDescriptor, u"COKPOWEHEU"); //Vendor
USB_STRING(USB_StringProdDescriptor, u"MAIL NOTIFER"); //Product
USB_STRING(USB_StringSerialDescriptor, u"2"); //Serial (BCD)
USB_STRING(USB_String_TTY, u"MAIL_LCD");

//#define USB_VID 0x16C0
//#define USB_PID 0x05DF
#define USB_VID 0x1294
#define USB_PID 0x1320

volatile uint8_t usb_class_mode = 0;

static const uint8_t USB_DeviceDescriptor[] = {
  ARRLEN1(
  bLENGTH,     // bLength
  USB_DESCR_DEVICE,   // bDescriptorType - Device descriptor
  USB_U16(0x0110), // bcdUSB
  DEVICE_CLASS_MISC,  // bDevice Class
  DEVICE_SUBCLASS_IAD,// bDevice SubClass
  DEVICE_PROTOCOL_IAD,// bDevice Protocol
  USB_EP0_BUFSZ,   // bMaxPacketSize0
  USB_U16( USB_VID ), // idVendor
  USB_U16( USB_PID ), // idProduct
  USB_U16( 1 ), // bcdDevice_Ver
  STD_DESCR_VEND,   // iManufacturer
  STD_DESCR_PROD,   // iProduct
  STD_DESCR_SN,   // iSerialNumber
  1    // bNumConfigurations
  )
};

static const uint8_t USB_DeviceQualifierDescriptor[] = {
  ARRLEN1(
  bLENGTH,     //bLength
  USB_DESCR_QUALIFIER,   // bDescriptorType - Device qualifier
  USB_U16(0x0200), // bcdUSB
  0,   // bDeviceClass
  0,   // bDeviceSubClass
  0,   // bDeviceProtocol
  USB_EP0_BUFSZ,   // bMaxPacketSize0
  1,   // bNumConfigurations
  0x00    // Reserved
  )
};


static const uint8_t USB_ConfigDescriptor[] = {
  ARRLEN34(
  ARRLEN1(
    bLENGTH, // bLength: Configuration Descriptor size
    USB_DESCR_CONFIG,    //bDescriptorType: Configuration
    wTOTALLENGTH, //wTotalLength
    interface_count, // bNumInterfaces
    1, // bConfigurationValue: Configuration value
    0, // iConfiguration: Index of string descriptor describing the configuration
    0x80, // bmAttributes: bus powered
    0x32, // MaxPower 100 mA
  )
  
  //HID
  ARRLEN1( //IAD
    bLENGTH,
    USB_DESCR_IAD, //bDescriptorType
    ifnum( interface_hid ), //bFirstInterface
    ifcnt( interface_hid ), //bInterfaceCount
    HIDCLASS_HID, // bInterfaceClass: 
    HIDSUBCLASS_NONE, // bInterfaceSubClass: 
    HIDPROTOCOL_NONE, // bInterfaceProtocol: 
    0, // iFuncion
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_INTERFACE, //bDescriptorType
    ifnum(interface_hid), //bInterfaceNumber
    0, // bAlternateSetting
    2, // bNumEndpoints
    HIDCLASS_HID, // bInterfaceClass: 
    HIDSUBCLASS_NONE, // bInterfaceSubClass: 
    HIDPROTOCOL_NONE, // bInterfaceProtocol: 
    0x00, // iInterface
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_HID, //bDescriptorType
    USB_U16(0x0110), //bcdHID
    0, //bCountryCode
    1, //bNumDescriptors
    USB_DESCR_HID_REPORT, //bDescriptorType
    USB_U16( sizeof(USB_HIDDescriptor) ), //wDescriptorLength
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_HID, //bEdnpointAddress
    USB_ENDP_INTR, //bmAttributes
    USB_U16( ENDP_HID_SIZE ), //MaxPacketSize
    10, //bInterval
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_HID | 0x80, //bEdnpointAddress
    USB_ENDP_INTR, //bmAttributes
    USB_U16( ENDP_HID_SIZE ), //MaxPacketSize
    10, //bInterval
  )
  
//ttyACM0 (interface 0, 1) - TTY
  ARRLEN1( //IAD
    bLENGTH,
    USB_DESCR_IAD, //bDescriptorType
    ifnum( interface_tty ), //bFirstInterface
    ifcnt( interface_tty ), //bInterfaceCount
    DEVCLASS_CDC, // bInterfaceClass: 
    CDCSUBCLASS_ACM, // bInterfaceSubClass: 
    CDCPROTOCOL_UNDEF, // bInterfaceProtocol: 
    STR_TTY, // iFuncion
  )
    ARRLEN1(//CDC control Interface
      bLENGTH, // bLength
      USB_DESCR_INTERFACE, // bDescriptorType
      ifnum( interface_tty ), //0, // bInterfaceNumber
      0, // bAlternateSetting
      1, // bNumEndpoints
      DEVCLASS_CDC, // bInterfaceClass: 
      CDCSUBCLASS_ACM, // bInterfaceSubClass: 
      CDCPROTOCOL_UNDEF, // bInterfaceProtocol: 
      STR_TTY, // iInterface
    )
      ARRLEN1(//Header
        bLENGTH, //bLength
        USB_DESCR_CS_INTERFACE, //bDescriptorType
        0, //bDescriptorSubType
        USB_U16(0x0110), //bcdCDC
      )
      ARRLEN1( //Call mamagement
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        0x01, //Call mamagement
        0x00, //TODO: bmCapabilities D0+D1
        ifnum( interface_tty )+1, //1, //bDataInterface
      )
      ARRLEN1( //ACM
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        2, //bDescriptorSubType = Abstract Control Management
        0x02, //bmCapabilities
          //7-4 : reserved
          //3   : supports 'Network connection'
          //2   : supports 'Send break'
          //1   : supports 'Set line coding', 'set control line state', 'get line coding', 'serial state'
          //0   : supports 'Set comm feature', 'clear comm feature', 'get comm feature'
      )
      ARRLEN1( //Union
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        6, //bDescriptorSubType = Union
        ifnum( interface_tty ), //0, //bMasterInterface = Communication class interface
        ifnum( interface_tty )+1, //1, //bSlaveInterface0 = Data Class Interface
      )
      ARRLEN1(//Endpoint descriptor
        bLENGTH, //bLength
        USB_DESCR_ENDPOINT, //bDescriptorType
        ENDP_TTY_CTL | 0x80,
        USB_ENDP_INTR,
        USB_U16(ENDP_CTL_SIZE),
        100, //bInterval - частота опроса, для изохронных всегда 1
      )
    ARRLEN1( //CDC data interface
      bLENGTH,
      USB_DESCR_INTERFACE,
      ifnum( interface_tty )+1, //1, //bInterfaceNumber
      0, //bAlternateSetting
      2, //bNumEndpoints
      CDCCLASS_DATA, //bInterfaceClass
      0, //bInterfaceSubClass
      0, //bInterfaceProtocol
      0, //iInterface
    )
    ARRLEN1(//Endpoint descriptor
      bLENGTH, //bLength
      USB_DESCR_ENDPOINT, //bDescriptorType
      ENDP_TTY_OUT,
      USB_ENDP_BULK,
      USB_U16(ENDP_TTY_SIZE),
      0, //bInterval - частота опроса, для изохронных всегда 1
    )
    ARRLEN1(//Endpoint descriptor
      bLENGTH, //bLength
      USB_DESCR_ENDPOINT, //bDescriptorType
      ENDP_TTY_IN | 0x80,
      USB_ENDP_BULK,
      USB_U16(ENDP_TTY_SIZE),
      0, //bInterval - частота опроса, для изохронных всегда 1
    )
  )
};

static const uint8_t USB_ConfigDescriptor_alt[] = {
  ARRLEN34(
  ARRLEN1(
    bLENGTH, // bLength: Configuration Descriptor size
    USB_DESCR_CONFIG,    //bDescriptorType: Configuration
    wTOTALLENGTH, //wTotalLength
    interface_count_alt, // bNumInterfaces
    1, // bConfigurationValue: Configuration value
    0, // iConfiguration: Index of string descriptor describing the configuration
    0x80, // bmAttributes: bus powered
    0x32, // MaxPower 100 mA
  )
  
  //HID
  ARRLEN1( //IAD
    bLENGTH,
    USB_DESCR_IAD, //bDescriptorType
    ifnum( interface_hid ), //bFirstInterface
    ifcnt( interface_hid ), //bInterfaceCount
    HIDCLASS_HID, // bInterfaceClass: 
    HIDSUBCLASS_NONE, // bInterfaceSubClass: 
    HIDPROTOCOL_NONE, // bInterfaceProtocol: 
    0, // iFuncion
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_INTERFACE, //bDescriptorType
    ifnum(interface_hid), //bInterfaceNumber
    0, // bAlternateSetting
    2, // bNumEndpoints
    HIDCLASS_HID, // bInterfaceClass: 
    HIDSUBCLASS_NONE, // bInterfaceSubClass: 
    HIDPROTOCOL_NONE, // bInterfaceProtocol: 
    0x00, // iInterface
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_HID, //bDescriptorType
    USB_U16(0x0110), //bcdHID
    0, //bCountryCode
    1, //bNumDescriptors
    USB_DESCR_HID_REPORT, //bDescriptorType
    USB_U16( sizeof(USB_HIDDescriptor) ), //wDescriptorLength
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_HID, //bEdnpointAddress
    USB_ENDP_INTR, //bmAttributes
    USB_U16( ENDP_HID_SIZE ), //MaxPacketSize
    10, //bInterval
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_HID | 0x80, //bEdnpointAddress
    USB_ENDP_INTR, //bmAttributes
    USB_U16( ENDP_HID_SIZE ), //MaxPacketSize
    10, //bInterval
  )
  
//ttyACM0 (interface 0, 1) - TTY
  ARRLEN1( //IAD
    bLENGTH,
    USB_DESCR_IAD, //bDescriptorType
    ifnum( interface_tty ), //bFirstInterface
    ifcnt( interface_tty ), //bInterfaceCount
    DEVCLASS_CDC, // bInterfaceClass: 
    CDCSUBCLASS_ACM, // bInterfaceSubClass: 
    CDCPROTOCOL_UNDEF, // bInterfaceProtocol: 
    STR_TTY, // iFuncion
  )
    ARRLEN1(//CDC control Interface
      bLENGTH, // bLength
      USB_DESCR_INTERFACE, // bDescriptorType
      ifnum( interface_tty ), //0, // bInterfaceNumber
      0, // bAlternateSetting
      1, // bNumEndpoints
      DEVCLASS_CDC, // bInterfaceClass: 
      CDCSUBCLASS_ACM, // bInterfaceSubClass: 
      CDCPROTOCOL_UNDEF, // bInterfaceProtocol: 
      STR_TTY, // iInterface
    )
      ARRLEN1(//Header
        bLENGTH, //bLength
        USB_DESCR_CS_INTERFACE, //bDescriptorType
        0, //bDescriptorSubType
        USB_U16(0x0110), //bcdCDC
      )
      ARRLEN1( //Call mamagement
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        0x01, //Call mamagement
        0x00, //TODO: bmCapabilities D0+D1
        ifnum( interface_tty )+1, //1, //bDataInterface
      )
      ARRLEN1( //ACM
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        2, //bDescriptorSubType = Abstract Control Management
        0x02, //bmCapabilities
          //7-4 : reserved
          //3   : supports 'Network connection'
          //2   : supports 'Send break'
          //1   : supports 'Set line coding', 'set control line state', 'get line coding', 'serial state'
          //0   : supports 'Set comm feature', 'clear comm feature', 'get comm feature'
      )
      ARRLEN1( //Union
        bLENGTH,
        USB_DESCR_CS_INTERFACE,
        6, //bDescriptorSubType = Union
        ifnum( interface_tty ), //0, //bMasterInterface = Communication class interface
        ifnum( interface_tty )+1, //1, //bSlaveInterface0 = Data Class Interface
      )
      ARRLEN1(//Endpoint descriptor
        bLENGTH, //bLength
        USB_DESCR_ENDPOINT, //bDescriptorType
        ENDP_TTY_CTL | 0x80,
        USB_ENDP_INTR,
        USB_U16(ENDP_CTL_SIZE),
        100, //bInterval - частота опроса, для изохронных всегда 1
      )
    ARRLEN1( //CDC data interface
      bLENGTH,
      USB_DESCR_INTERFACE,
      ifnum( interface_tty )+1, //1, //bInterfaceNumber
      0, //bAlternateSetting
      2, //bNumEndpoints
      CDCCLASS_DATA, //bInterfaceClass
      0, //bInterfaceSubClass
      0, //bInterfaceProtocol
      0, //iInterface
    )
    ARRLEN1(//Endpoint descriptor
      bLENGTH, //bLength
      USB_DESCR_ENDPOINT, //bDescriptorType
      ENDP_TTY_OUT,
      USB_ENDP_BULK,
      USB_U16(ENDP_TTY_SIZE),
      0, //bInterval - частота опроса, для изохронных всегда 1
    )
    ARRLEN1(//Endpoint descriptor
      bLENGTH, //bLength
      USB_DESCR_ENDPOINT, //bDescriptorType
      ENDP_TTY_IN | 0x80,
      USB_ENDP_BULK,
      USB_U16(ENDP_TTY_SIZE),
      0, //bInterval - частота опроса, для изохронных всегда 1
    )

//MSD
  ARRLEN1( //IAD
    bLENGTH,
    USB_DESCR_IAD, //IAD descriptor
    ifnum( interface_msd ), //bFirstInterface
    ifcnt( interface_msd ), //bInterfaceCount
    MSDCLASS_MSD, // bInterfaceClass:
    MSDSUBCLASS_SCSI, // bInterfaceSubClass:
    MSDPROTOCOL_BULKONLY, // bInterfaceProtocol:
    0, //0x00, // iFuncion
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_INTERFACE, //bDescriptorType
    ifnum( interface_msd ),// bInterfaceNumber
    0, // bAlternateSetting
    2, // bNumEndpoints
    MSDCLASS_MSD, // bInterfaceClass:
    MSDSUBCLASS_SCSI, // bInterfaceSubClass:
    MSDPROTOCOL_BULKONLY, // bInterfaceProtocol:
    0, // iInterface
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_MSD_IN | 0x80,  //Endpoint address
    USB_ENDP_BULK,   //Bulk endpoint type
    USB_U16(ENDP_MSD_SIZE), //endpoint size
    0x00,   //Polling interval in milliseconds (ignored)
  )
  ARRLEN1(
    bLENGTH, //bLength
    USB_DESCR_ENDPOINT, //bDescriptorType
    ENDP_MSD_OUT,  //Endpoint address
    USB_ENDP_BULK,   //Bulk endpoint type
    USB_U16(ENDP_MSD_SIZE), //endpoint size
    0x00,   //Polling interval in milliseconds (ignored)
  )

  )
};

void usb_class_get_std_descr(uint16_t descr, const void **data, uint16_t *size){
  switch(descr & 0xFF00){
    case DEVICE_DESCRIPTOR:
      *data = &USB_DeviceDescriptor;
      *size = sizeof(USB_DeviceDescriptor);
      break;
    case CONFIGURATION_DESCRIPTOR:
      if(usb_class_mode == 0){
        *data = &USB_ConfigDescriptor;
        *size = sizeof(USB_ConfigDescriptor);
      }else{
        *data = &USB_ConfigDescriptor_alt;
        *size = sizeof(USB_ConfigDescriptor_alt);
      }
      break;
    case DEVICE_QUALIFIER_DESCRIPTOR:
      *data = &USB_DeviceQualifierDescriptor;
      *size = USB_DeviceQualifierDescriptor[0];
      break;
    case STRING_DESCRIPTOR:
      switch(descr & 0xFF){
        case STD_DESCR_LANG:
          *data = &USB_StringLangDescriptor;
          break;
        case STD_DESCR_VEND:
          *data = &USB_StringManufacturingDescriptor;
          break;
        case STD_DESCR_PROD:
          *data = &USB_StringProdDescriptor;
          break;
        case STD_DESCR_SN:
          *data = &USB_StringSerialDescriptor;
          break;
        case STR_TTY:
          *data = &USB_String_TTY;
          break;
        default:
          return;
      }
      *size = ((uint8_t*)*data)[0]; //data->bLength
      break;
    default:
      break;
  }
}

char usb_class_ep0_in(config_pack_t *req, void **data, uint16_t *size){
  if( hid_ep0_in( req, data, size ) )return 1;
  if( tty_ep0_in( req, data, size ) )return 1;
  if( msd_ep0_in( req, data, size ) )return 1;
  return 0;
}

char usb_class_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size){
  if( hid_ep0_out( req, offset, rx_size ) )return 1;
  if( tty_ep0_out( req, offset, rx_size ) )return 1;
  if( msd_ep0_out( req, offset, rx_size ) )return 1;
  return 0;
}

void usb_class_init(){
  hid_init();
  tty_init();
  msd_init();
}

void usb_class_poll(){
  hid_poll();
  tty_poll();
  msd_poll();
}
