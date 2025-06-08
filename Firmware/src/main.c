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

//while true ; do echo -e "\e[1;1H" > /dev/tty_MAIL_LCD_0 ; date "+%H:%M:%S           " > /dev/tty_MAIL_LCD_0 ; date "+%d.%m.%Y" | tr -d '\n' > /dev/tty_MAIL_LCD_0 ; sleep 1 ; done

//while true ; do date "+_%H:%M:%S_%d.%m.%Y" | tr -d '\n' | sed 's/_/\n/g' > /dev/tty_MAIL_LCD_0 ; sleep 1 ; done

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

  lcd_puts("\e[J\e[1;1H"__TIME__);
  
  UART_init(USART, 144000000 / 2 / 115200);
  UART_puts(USART, __TIME__ " " __DATE__ "\r\n");
  
  usb_class_mode = 0;
  if(GPI_ON(BTN))usb_class_mode = 1;
  
  USB_setup(); //USB_device
  uint8_t cont = 0;
  uint32_t time;
  
  
  while(1){
    usb_class_poll();
    time = systick_read32();
    
    int16_t ch = UART_getc(USART);
    if(ch > 0)lcd_putc(ch);
    
    lcd_update(time, adc_raw[0].Ucont);
    snd_poll(time);
    
#if 0
    if(reinit_flag){
      USB_setup();
      reinit_flag = 0;
    }
#endif
  }
}
