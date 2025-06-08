#include "ch32v20x.h"
#include "hardware.h"
#include "pinmacro.h"
#include "clock.h"
#define USART 2
#define UART_SIZE_PWR 8
#include "uart.h"
#include "strlib.h"

//TODO: Запись во флеш
//TODO: Отличить долгое нажатие на кнопку

//Запись флеша возможна на скорости не более 60 МГц. Отключить USB; снизить частоту; записать значения; восстановить частоту; включить USB. Все равно нужно будет реконнектить.

void SystemInit(void){}

#define MEM_OFFSET	0x08000000
#define MEM_START	(40*1024) //0x00 A000
#define MEM_END		(64*1023) //0x01 0000
#define MEM_MASK	0xE339
#define MEM_XOR		0x1CC6

void flash_unlock(){
  //TODO: change F_CPU
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
}

void flash_lock(){
  FLASH->CTLR |= FLASH_CTLR_LOCK;
}

void flash_erase_4K(uint32_t addr){
  FLASH->CTLR |= FLASH_CTLR_PER;
  addr |= MEM_OFFSET;
  addr &=~ 4095LLU;
  FLASH->ADDR = addr;
  FLASH->CTLR |= FLASH_CTLR_STRT;
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  FLASH->CTLR &=~ FLASH_CTLR_PER;
}

void flash_w16(uint32_t addr, uint16_t data){
  FLASH->CTLR |= FLASH_CTLR_PG;
  addr |= MEM_OFFSET;
  ((uint16_t*)addr)[0] = data;
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  FLASH->CTLR &=~ FLASH_CTLR_PG;
}

char* str_readhex(char *str, uint32_t *res){
  uint32_t val = 0;
  str--;
  while(1){
    str++;
    if( (str[0]==' ')||(str[0]=='\t') )continue;
    if( (str[0]=='\r')||(str[0]=='\n')||(str[0]==0) )return NULL;
    break;
  }
  if((str[0] == '0')&&(str[1]=='x'))str += 2;
  while(1){
    if( (str[0]>='0')&&(str[0]<='9') ){
      val = val * 16 + str[0] - '0';
    }else if( (str[0]>='a')&&(str[0]<='f') ){
      val = val * 16 + str[0] - 'a' + 0xA;
    }else if( (str[0]>='A')&&(str[0]<='F') ){
      val = val * 16 + str[0] - 'A' + 0xA;
    }else break;
    str++;
  }
  *res = val;
  return str;
}

void cmd_d(char *cmd){
  uint32_t addr = 0;
  cmd = str_readhex(cmd, &addr);
  addr |= MEM_OFFSET;
  while(UART_avaible(USART) < 200){}
  UART_puts(USART, "cmd: dump 0x");
  UART_puts(USART, u32tohex(NULL, addr, 8));
  UART_puts(USART, "\r\n");
  for(int i=0; i<256; i++){
    while(UART_avaible(USART) < 200){}
    UART_puts(USART, u32tohex(NULL, ((uint8_t*)(addr + i))[0], 2));
    UART_puts(USART, " ");
  }
  UART_puts(USART, "\r\n");
}
//void flash_w16(uint32_t addr, uint16_t data)
void cmd_w(char *cmd){
  uint32_t addr = 0;
  cmd = str_readhex(cmd, &addr);
  uint8_t buf[100];
  int cnt = 0;
  uint32_t temp;
  while(1){
    temp = 0;
    cmd = str_readhex(cmd, &temp);
    if(cmd == NULL)break;
    buf[cnt] = temp;
    cnt++;
    if(cnt >= sizeof(buf))break;
  }
  
  while(UART_avaible(USART) < 200){}
  if((addr < MEM_START)||(addr > MEM_END) ){
    UART_puts(USART, "write error: wrong address 0x");
    UART_puts(USART, u32tohex(NULL, addr, 8));
    UART_puts(USART, "\r\n");
    return;
  }
  
  
  UART_puts(USART, "cmd: write 0x");
  UART_puts(USART, u32tohex(NULL, addr, 8));
  UART_puts(USART, " ");
  cnt = (cnt+1)/2;
  for(int i=0; i<cnt; i++){
    uint16_t *arr = (uint16_t*)buf;
    flash_w16(addr+i*2, arr[i]);
    while(UART_avaible(USART) < 200){}
    UART_puts(USART, u32tohex(NULL, arr[i], 4));
    UART_puts(USART, " ");
  }
  UART_puts(USART, "\r\n");
}

void cmd_e(char *cmd){
  uint32_t addr = 0;
  cmd = str_readhex(cmd, &addr);
  while(UART_avaible(USART) < 200){}
  if((addr < MEM_START)||(addr > MEM_END) ){
    UART_puts(USART, "erase error: wrong address 0x");
    UART_puts(USART, u32tohex(NULL, addr, 8));
    UART_puts(USART, "\r\n");
    return;
  }
  UART_puts(USART, "cmd: erase 0x");
  UART_puts(USART, u32tohex(NULL, addr, 8));
  UART_puts(USART, "\r\n");
}

//0 - released
//>0 - pressed <time>
//<0 - was pressed <time>. Now released
int32_t btn_pressed_time(uint32_t time){
  static uint32_t t_press = 0;
  static uint8_t prev = 0;
  if(GPI_ON(BTN)){
    if(prev)return (time - t_press);
    prev = 1;
    t_press = time;
    return 1;
  }else{
    if(!prev)return 0;
    prev = 0;
    return (t_press - time);
  }
}

void clock_hsi(){
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | RCC_SW_HSI;
  while((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI){}
}

int main(){
  clock_HS(1);
  systick_init();
  RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPBEN | RCC_AFIOEN;
  GPIO_config(BTN);
  GPIO_manual(SND, GPIO_PP50); GPO_OFF(SND);
  GPIO_manual(LCD_CONT, GPIO_PP50); GPO_OFF(LCD_CONT);
  GPIO_manual(LCD_BL, GPIO_PP50); GPO_OFF(LCD_BL);
  GPIO_manual(RLED, GPIO_PP50); GPO_OFF(RLED);
  GPIO_manual(GLED, GPIO_PP50); GPO_OFF(GLED);
  GPIO_manual(BLED, GPIO_PP50); GPO_OFF(BLED);
  
  UART_init(USART, 144000000 / 2 / 115200);
  UART_puts(USART, __TIME__ " " __DATE__ "\r\n");
  
  uint32_t time;
  uint8_t clock_hs = 1;
  uint32_t f_cpu = 144000000;
  while(1){
    time = systick_read32();
    PERIOD_BLOCK(time, 144000000 / 20){
      if(clock_hs)GPO_ON(RLED); else GPO_OFF(RLED);
      GPO_T(GLED);
      
      int32_t btn = btn_pressed_time(time);
      UART_puts(USART, fpi32tos(NULL, btn, 0, 0));
      UART_puts(USART, "\r\n");
      if(btn < 0){
        if(clock_hs){
          clock_hs = 0;
          f_cpu = 8000000;
          clock_hsi();
        }else{
          clock_hs = 1;
          f_cpu = 144000000;
          clock_HS(0);
        }
      }
    }
    //if(GPI_ON(BTN))GPO_ON(RLED); else GPO_OFF(RLED);
  }
}
#if 0
int main1(){
  //clock_HS(1);
  systick_init();
  RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPBEN | RCC_AFIOEN;
  GPIO_config(BTN);
  GPIO_manual(SND, GPIO_PP50); GPO_OFF(SND);
  GPIO_manual(LCD_CONT, GPIO_PP50); GPO_OFF(LCD_CONT);
  GPIO_manual(LCD_BL, GPIO_PP50); GPO_OFF(LCD_BL);
  GPIO_manual(RLED, GPIO_PP50); GPO_OFF(RLED);
  GPIO_manual(GLED, GPIO_PP50); GPO_OFF(GLED);
  GPIO_manual(BLED, GPIO_PP50); GPO_OFF(BLED);
  
  //UART_init(USART, 144000000 / 2 / 115200);
  UART_init(USART, 8000000 / 115200);
  UART_puts(USART, __TIME__ " " __DATE__ "\r\n");
  
  flash_unlock();
  
  uint32_t time;
  while(1){
    time = systick_read32();
    
    PERIOD_BLOCK(time, 8000000 / 10){
      GPO_T(GLED);
    }
    
    int len = UART_str_size(USART);
    if(len < 0){UART_rx_clear(USART); continue;}
    if(len == 0)continue;
    char buf[200];
    UART_read(USART, (uint8_t*)buf, len);
    buf[len] = 0;
    switch(buf[0]){
      case 'l': GPO_T(RLED); break;
      case 'd': cmd_d(buf+1); break;
      case 'w': cmd_w(buf+1); break;
      case 'e': cmd_e(buf+1); break;
      default: UART_puts(USART, "Error\r\n");
    }
    UART_rx_clear(USART);
  }
  return 0;
}
#endif