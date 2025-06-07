#include "tty.h"
#include "clock.h"
#include "lcd_hd44780.h"

//#define UART_DECLARATIONS 2
//#include "uart.h"

#ifndef NULL
  #define NULL ((void*)0)
#endif

#define CDC_SEND_ENCAPSULATED 0x00
#define CDC_GET_ENCAPSULATED  0x01
#define CDC_SET_COMM_FEATURE  0x02
#define CDC_GET_COMM_FEATURE  0x03
#define CDC_CLR_COMM_FEATURE  0x04
#define CDC_SET_LINE_CODING   0x20
#define CDC_GET_LINE_CODING   0x21
#define CDC_SET_CTRL_LINES    0x22
#define CDC_SEND_BREAK        0x23

typedef union{
  uint16_t raw[7];
  struct{
    uint32_t baudrate;
    uint8_t stopbits; //0=1bit, 1=1.5bits, 2=2bits
    uint8_t parity; //0=none, 1=odd, 2=even, 3=mark (WTF?), 4=space (WTF?)
    uint8_t wordsize; //length of data word: 5,6,7,8 or 16 bits
  }__attribute__((packed));
} cdc_linecoding;

USB_ALIGN volatile cdc_linecoding tty_cfg = {
  .baudrate = 9600,
  .stopbits = 1,
  .parity = 0,
  .wordsize = 8,
};

#define SPEED_CTRL	50
#define TIMEOUT_MS 	10000
#define systick_ms() (systick_read32() / (144000000 / 1000))
#define timeout_reset() do{timeout_ms = systick_ms() + TIMEOUT_MS;}while(0)

void uart_ctrl( uint8_t *buf, int size);

char tty_ep0_in(config_pack_t *req, void **data, uint16_t *size){
  if( (req->bmRequestType & 0x7F) == (USB_REQ_CLASS | USB_REQ_INTERFACE) ){
    if( req->bRequest == CDC_GET_LINE_CODING ){
      if( req->wIndex == ifnum(interface_tty) ){
        *data = (void*)&tty_cfg;
        *size = sizeof(tty_cfg);
        return 1;
      }
    }
  }
  return 0;
}

char tty_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size){
  if( (req->bmRequestType & 0x7F) == (USB_REQ_CLASS | USB_REQ_INTERFACE) ){
    if( req->bRequest == CDC_SET_LINE_CODING ){
      if(rx_size == 0)return 1;
      if( req->wIndex == ifnum(interface_tty) ){
        usb_ep_read(0, (uint16_t*)&tty_cfg);
        return 1;
      }
    }else if( req->bRequest == CDC_SET_CTRL_LINES ){
      return 1; //read wValue to RTS, DTR
    }
  }
  return 0;
}

static void tty_out(uint8_t epnum){
  USB_ALIGN uint8_t buf[ ENDP_TTY_SIZE ];
  uint32_t len = usb_ep_read( ENDP_TTY_OUT, (uint16_t*)buf );
  for(int i=0; i<len; i++){
    //UART_puts(UART_DECLARATIONS, u32tohex(NULL, buf[i], 2));  UART_puts(UART_DECLARATIONS, " ");
    lcd_putc(buf[i]);
  }
}

void tty_init(){
  usb_ep_init( ENDP_TTY_CTL  | 0x80, USB_ENDP_INTR, ENDP_CTL_SIZE,  NULL );
  usb_ep_init( ENDP_TTY_IN   | 0x80, USB_ENDP_BULK, ENDP_TTY_SIZE,  NULL );
  usb_ep_init( ENDP_TTY_OUT,         USB_ENDP_BULK, ENDP_TTY_SIZE,  tty_out );
}

void tty_poll(){
  if( usb_ep_ready( ENDP_TTY_OUT ) ){
    tty_out( ENDP_TTY_OUT );
  }
}