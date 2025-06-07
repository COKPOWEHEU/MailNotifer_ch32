#ifndef __USB_CLASS_HID__
#define __USB_CLASS_HID__

#include "hardware.h"
#include "timer.h"
#include "usb_hid.h"
#include "strlib.h"

char hid_ep0_in(config_pack_t *req, void **data, uint16_t *size);
char hid_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size);
void hid_init();
void hid_poll();

void led_init();
extern volatile uint8_t colortable[8];
extern volatile uint8_t colorpwm[3];

void vf_rgb_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);
void vf_rgb_write(uint8_t *buf, uint32_t addr, uint16_t file_idx);

#include "usb_class_virfat.h"

#define rgb_virfat_files \
{ \
  .name = "RGB_CFG CFG", \
  .file_read = vf_rgb_read, \
  .file_write = vf_rgb_write, \
  .size = 1, \
}, \


#ifdef USB_CLASS_HID_IMPLEMENTATION

USB_ALIGN static const uint8_t USB_HIDDescriptor[] = {
  0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,        // Usage (0x01)
  0xA1, 0x01,        // Collection (Application)
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (0x01)
  0x29, 0x01,        //   Usage Maximum (0x01)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x01,        //   Report Size (1)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x07,        //   Report Size (7)
  0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  
  0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
  0x09, 0x46,        //   Usage (Vno)
  0x16, 0xDA, 0x00,  //   Logical Minimum (218)
  0x26, 0x89, 0x01,  //   Logical Maximum (393)
  0x67, 0x01, 0x00, 0x01, 0x00,  //   Unit (System: SI Linear, Temperature: Kelvin)
  0x55, 0x00,        //   Unit Exponent (0)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x38,        //   Report Size (56)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  
  0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,        //   Usage (0x01)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x7F,        //   Logical Maximum (127)
  0x95, 0x05,        //   Report Count (5)
  0x75, 0x08,        //   Report Size (8)
  0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  0xC0,              // End Collection
};

char hid_ep0_in(config_pack_t *req, void **data, uint16_t *size){
  if( req->bmRequestType == (USB_REQ_INTERFACE | 0x80) ){
    if( req->bRequest == GET_DESCRIPTOR ){
      if( req->wValue == HID_REPORT_DESCRIPTOR){
        *data = (void**)&USB_HIDDescriptor;
        *size = sizeof(USB_HIDDescriptor);
        return 1;
      }
    }
  }
  return 0;
}

char hid_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size){
  return 0;
}

//b2-R, b1-G, b0-B
//                                  -       R     G        B     GB    RG     R B    RGB
volatile uint8_t colortable[8] = {0b000, 0b010, 0b100, 0b001, 0b011, 0b110, 0b101, 0b111};
volatile uint8_t colorpwm[3] = {100, 100, 100};
#define RGBTIM_MAX 255
#warning TODO: add settings into VirFat
#warning TODO: FLASH

void led_init(){
  GPIO_config(RLED); GPIO_config(GLED); GPIO_config(BLED);
  timer_init(RTIM, 0, RGBTIM_MAX);
  timer_chval(RTIM)=0; timer_chval(GTIM)=0; timer_chval(BTIM)=0;
  timer_chcfg(RTIM); timer_chcfg(GTIM); timer_chcfg(BTIM);
  timer_enable(RTIM);
}

void hid_ep_out(uint8_t epnum){
  USB_ALIGN uint8_t buf[ENDP_HID_SIZE];
  usb_ep_read(ENDP_HID, (uint16_t*)buf);
  uint8_t col = buf[0]; if(col > 7)col = 0;
  col = colortable[col];
  if(col & 0b100)timer_chval(RTIM) = colorpwm[0]; else timer_chval(RTIM)=0;
  if(col & 0b010)timer_chval(GTIM) = colorpwm[1]; else timer_chval(GTIM)=0;
  if(col & 0b001)timer_chval(BTIM) = colorpwm[2]; else timer_chval(BTIM)=0;
}

void hid_init(){
  usb_ep_init( ENDP_HID,        USB_ENDP_INTR, ENDP_HID_SIZE, hid_ep_out);
  usb_ep_init( ENDP_HID | 0x80, USB_ENDP_INTR, ENDP_HID_SIZE, NULL);
}

void hid_poll(){}



//////// VirFat ////////////////////////

//R (0 - 100): 100 \r\n
//G (0 - 100): 100 \r\n
//B (0 - 100): 100 \r\n
//
//Color codes:
//0: ___ \r\n
//1: R__ \r\n
//...
//7: RGB \r\n

#define R_STR         "R (0 - 100): "
#define G_STR "100 \r\nG (0 - 100): "
#define B_STR "100 \r\nB (0 - 100): "
#define C_STR "100 \r\n\r\nColor codes:\r\n"
#define RG_STR R_STR G_STR
#define RGB_STR R_STR G_STR B_STR
#define COMM_STR R_STR G_STR B_STR C_STR
//volatile uint8_t colortable[8] = {0b000, 0b010, 0b100, 0b001, 0b011, 0b110, 0b101, 0b111};
void vf_rgb_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  const char bristr[] = COMM_STR;
  char *ch = (char*)buf;
  uint8_t r,g,b;
  r = colorpwm[0] * 100 / RGBTIM_MAX;
  g = colorpwm[1] * 100 / RGBTIM_MAX;
  b = colorpwm[2] * 100 / RGBTIM_MAX;
  memcpy(ch, (char*)bristr, sizeof(bristr));
  fpi32tos_inplace( ch + sizeof(R_STR)-1, r, 0, 3 );
  fpi32tos_inplace( ch + sizeof(RG_STR)-1, g, 0, 3 );
  fpi32tos_inplace( ch + sizeof(RGB_STR)-1, b, 0, 3 );
  ch += sizeof(bristr) - 1;
  
  for(int i=0; i<sizeof(colortable); i++){
    ch[0] = i+'0';
    ch[1] = ':';
    ch[2] = ' ';
    int col = colortable[i];
    if(col & 0b100)ch[3]='R'; else ch[3]='_';
    if(col & 0b010)ch[4]='G'; else ch[4]='_';
    if(col & 0b001)ch[5]='B'; else ch[5]='_';
    ch[6] = ch[7] = ' ';
    ch[8] = '\r'; ch[9] = '\n';
    ch += 10;
  }
  
  memset(ch, ' ', (char*)buf + 512 - ch);
}

void vf_rgb_write(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  char *ch;
  char *en = (char*)buf + 512;
  uint32_t r=0, g=0, b=0;
  
  ch = strstr((char*)buf, "R (0 - 100):");
  if(ch == NULL)return;
  ch += sizeof("R (0 - 100):")-1;
  while( (ch[0]==' ')||(ch[0]=='\t') ){if(ch[0]==0)return; ch++; }
  while( (ch[0]>='0')&&(ch[0]<='9') ){
    r = r*10 + ch[0] - '0';
    ch++;
  }
  if(r > 100)r = 100;
  
  ch = strstr((char*)buf, "G (0 - 100):");
  if(ch == NULL)return;
  ch += sizeof("G (0 - 100):")-1;
  while( (ch[0]==' ')||(ch[0]=='\t') ){if(ch[0]==0)return; ch++; }
  while( (ch[0]>='0')&&(ch[0]<='9') ){
    g = g*10 + ch[0] - '0';
    ch++;
  }
  if(g > 100)g = 100;
  
  ch = strstr((char*)buf, "B (0 - 100):");
  if(ch == NULL)return;
  ch += sizeof("B (0 - 100):")-1;
  while( (ch[0]==' ')||(ch[0]=='\t') ){if(ch[0]==0)return; ch++; }
  while( (ch[0]>='0')&&(ch[0]<='9') ){
    b = b*10 + ch[0] - '0';
    ch++;
  }
  if(b > 100)b = 100;
  
  colorpwm[0] = r * RGBTIM_MAX / 100;
  if(timer_chval(RTIM)!=0)timer_chval(RTIM) = colorpwm[0];
  colorpwm[1] = g * RGBTIM_MAX / 100;
  if(timer_chval(GTIM)!=0)timer_chval(GTIM) = colorpwm[1];
  colorpwm[2] = b * RGBTIM_MAX / 100;
  if(timer_chval(BTIM)!=0)timer_chval(BTIM) = colorpwm[2];
  
  ch = strstr((char*)buf, "Color codes:");
  if(ch == NULL)return;
  ch += sizeof("Color codes:");
  unsigned int cols[ sizeof(colortable) ];
  int cnt = 0, idx = 0;
  memset(cols, 0, sizeof(cols));
  
  while((ch < en) && (cnt < sizeof(colortable))){
    for(; (ch[0]==' ')||(ch[0]=='\t')||(ch[0]=='\r')||(ch[0]=='\n'); ch++){
      if(ch >= en)break;
    }
    if((ch[0] < '0')||(ch[0]>'7'))return;
    idx = ch[0]-'0';
    
    ch += 2;
    unsigned int col = 0b1000; //flag that this color exist in file
    for(; (ch[0]!='\r')&&(ch[0]!='\n'); ch++){
      if(ch >= en)break;
      if((ch[0]=='r')||(ch[0]=='R')){col |= 0b100; continue;}
      if((ch[0]=='g')||(ch[0]=='G')){col |= 0b010; continue;}
      if((ch[0]=='b')||(ch[0]=='B')){col |= 0b001; continue;}
    }
    cols[idx] = col;
    cnt++;
  }
  for(int i=0; i<sizeof(colortable); i++){
    if(cols[i] == 0)return; // if at least one color does not exist in file - error
  }
  for(int i=0; i<sizeof(colortable); i++){
    colortable[i] = cols[i] & 0b111;
  }
}

#endif

#endif