#include "ch32v20x.h"
#include "hardware.h"
#include "pinmacro.h"
#include "clock.h"
#include "usbd_lib.h"
#define USART 2
#define UART_SIZE_PWR 8
#include "uart.h"
#include "usb_class_virfat.h"
#include "dma.h"
#include "adc.h"
#include "usb_class_hid.h"
#include "strlib.h"
#include "lcd_hd44780.h"
#include "beep.h"
#define EEFLASH_SIZE		(2*4096)
#define EEFLASH_DATASIZE 	sizeof(device_settings_t)
#define EEFLASH_ADDRESS		0x08036000 //non-zero-wait area (244k) - (2*4k, eeflash_size) - (2*4k, just in case)
#define EEFLASH_IMPLEMENTATION
#include "eeflash.h"
#include "savedata.h"

//while true ; do echo -e "\e[1;1H" > /dev/tty_MAIL_LCD_0 ; date "+%H:%M:%S           " > /dev/tty_MAIL_LCD_0 ; date "+%d.%m.%Y" | tr -d '\n' > /dev/tty_MAIL_LCD_0 ; sleep 1 ; done

//while true ; do date "+_%H:%M:%S_%d.%m.%Y" | tr -d '\n' | sed 's/_/\n/g' > /dev/tty_MAIL_LCD_0 ; sleep 1 ; done

//default values
device_settings_t device_settings = {
  .lcd = {
    .contrast = 0,
    .backlight = 10,
    .cursor = CUR_NONE,
    .newline_mode = NL_NEWLINE,
  },
  .rgb = {
    .colortable = {0b000, 0b010, 0b100, 0b001, 0b011, 0b110, 0b101, 0b111},
    .colorpwm = {100, 100, 100},
  },
  .snd = {.freq_Hz = 500, .dur_ms = 1000, .vol = 10,}
};

void SystemInit(void){}

struct{
  uint16_t Ucont;
}adc_raw[1];

void adc_init(){
  ADC_init(ADC_1, ADC_TS_VREF_EN, 8);
  ADC_SAMPLING_TIME( ADC_1, marg5(LCD_CONT_ADC), ADC_SAMPL_TIME_72 );
  ADC_SEQ_SET( ADC_1, 1, marg5(LCD_CONT_ADC) );
  
  ADC_SEQ_CNT( ADC_1, 1 );
  ADC_Trigger( ADC_1, ADC_TRIG_DISABLE );
  ADC_SCANMODE( ADC_1, ADC_SCAN_SEQ, ADC_SCAN_CYCLE );
  
  dma_clock( ADC1_DMA, 1 );
  dma_cfg_io( ADC1_DMA, &adc_raw, &ADC_data(ADC_1), (sizeof(adc_raw)/(sizeof(uint16_t))) );
  dma_cfg_mem( ADC1_DMA, 16,1, 16,0, 1, DMA_PRI_LOW );
  dma_enable( ADC1_DMA );
  ADC_start( ADC_1 );
}

// TSND, LCD_BL_TIM = Tim1
// LCD_CONT_TIM = Tim2
void tim_init(){
  timer_init(TSND, 5, 255); //same LCD_BL
  timer_chval(TSND)=0;
  timer_chcfg(TSND);
  GPIO_config(SND);
  TIMO_OFF(TSND);
  
  timer_init(LCD_CONT_TIM, 20, 255);
  timer_chval(LCD_CONT_TIM)=0;
  timer_chcfg(LCD_CONT_TIM);
  
  timer_enable(TSND);
  timer_enable(LCD_CONT_TIM);
}

#define BTN_MIN_ticks	(144000000 / 1000)
#define BTN_LONG_ticks	(144000000)
//0 - released
//<0 - pressed <time>
//>0 - was pressed <time>. Now released
int32_t btn_pressed_time(uint32_t time){
  static uint32_t t_press = 0;
  static uint8_t prev = 0;
  if(GPI_ON(BTN)){
    if(prev)return (t_press - time);
    prev = 1;
    t_press = time;
    return -1;
  }else{
    if(!prev)return 0;
    prev = 0;
    return (time - t_press);
  }
}

int main(){
  clock_HS(1); systick_init();
  RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPBEN | RCC_AFIOEN;
  GPIO_config(BTN);
  GPIO_config(SND); GPO_OFF(SND);
  led_init();
  lcd_init();
  tim_init();
  adc_init();
  snd_init();
  lcd_beep_func = snd_start;
  //lcd_puts("\e[J\e[1;1H"__TIME__"\e1;1H");
  lcd_puts("\e[J\e[1;1H_\e1;1H");
  
  UART_init(USART, 144000000 / 2 / 115200);
  UART_puts(USART, __TIME__ " " __DATE__ "\r\n");
  
  eeflash_init();
#if 0
  //on first burning
  eeflash_erase();
  eeflash_write(&device_settings);
#endif

  //if the button is pressed when power on - ask for reset settings to default
  if(GPI_ON(BTN)){
    lcd_puts("\e[1JRst? Btn off-Y");
    lcd_puts("\e[2HDisconnect - N");
    while(GPI_ON(BTN)){
      uint32_t time = systick_read32();
      lcd_update(time, adc_raw[0].Ucont);
    }
    delay_ticks(144000000);
    eeflash_write(&device_settings);
    
    lcd_puts("\e[1JSettings restor");
    lcd_puts("\e[2Hed to default");
    uint32_t t_av = systick_read32() + 144000000 * 5;
    uint32_t t;
    do{
      t = systick_read32();
      lcd_update(t, adc_raw[0].Ucont);
    }while( (int32_t)(t - t_av) < 0 );
    lcd_puts("\e[J");
  }
  
  eeflash_read(&device_settings);
  lcd_bl(device_settings.lcd.backlight);
  
  usb_class_mode = 0;
  
  USB_setup(); //USB_device

  uint32_t time;
  while(1){
    usb_class_poll();
    time = systick_read32();
    
    lcd_update(time, adc_raw[0].Ucont);
    snd_poll(time);
    
    
    int32_t btn = btn_pressed_time(time);
    if( btn > BTN_MIN_ticks ){
      if(usb_class_mode == 0){
        usb_class_mode = 1;
        USB_setup();
      }else{
        if( btn < BTN_LONG_ticks ){
          eeflash_read(&device_settings);
          lcd_temptext = "undo changes    ";
          led_init();
          lcd_bl(device_settings.lcd.backlight);
        }else{
          eeflash_write(&device_settings);
          lcd_temptext = "saved           ";
        }
        usb_class_mode = 0;
        USB_setup();
      }
    }
    
  }
}
