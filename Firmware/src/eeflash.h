#if 1==0
#define EEFLASH_SIZE		(2*4096)
#define EEFLASH_DATASIZE 	100
#define EEFLASH_ADDRESS		0x0800C000 //64k - 2k(eeflash_size) - 2k(just in case)
#define EEFLASH_IMPLEMENTATION
#include "eeflash.h"

#endif



#ifndef EEFLASH_IMPLEMENTATION
#ifndef __EEFLASH_H__
#define __EEFLASH_H__

#if defined(EEFLASH_SIZE||defined(EEFLASH_DATASIZE)
#error EEFLASH_SIZE and EEFLASH_DATASIZE may be defined with EEFLASH_IMPLEMENTATION only
#endif




#endif //__EEFLASH_H__
#else //EEFLASH_IMPLEMENTATION

#if !defined(EEFLASH_SIZE) || (EEFLASH_SIZE < (2*4096))
  #error define EEFLASH_SIZE more than 2x4k
#endif

#define MEM_XOR		0xE339
#define MEM_XOR32	0xE339E339

static void flash_unlock(){
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
}

static void flash_lock(){
  FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static void flash_erase_4K(uint32_t addr){
  FLASH->CTLR |= FLASH_CTLR_PER;
  addr &=~ 4095LLU;
  FLASH->ADDR = addr;
  FLASH->CTLR |= FLASH_CTLR_STRT;
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  FLASH->CTLR &=~ FLASH_CTLR_PER;
}

static void flash_w16(uint32_t addr, uint16_t data){
  FLASH->CTLR |= FLASH_CTLR_PG;
  ((uint16_t*)addr)[0] = data;
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  FLASH->CTLR &=~ FLASH_CTLR_PG;
}

static void flash_w32(uint32_t addr, uint32_t data){
  typedef union{
    uint32_t v32;
    uint16_t v16[2];
  }u16_32_t;
  FLASH->CTLR |= FLASH_CTLR_PG;
  ((uint16_t*)addr)[0] = ((u16_32_t*)&data)->v16[0];
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  ((uint16_t*)addr)[1] = ((u16_32_t*)&data)->v16[1];
  while( FLASH->STATR & FLASH_STATR_BSY ){}
  FLASH->STATR = FLASH_STATR_EOP;
  FLASH->CTLR &=~ FLASH_CTLR_PG;
}


typedef struct{
  uint32_t wnum;
  uint16_t data[ (EEFLASH_DATASIZE+1)/2 ];
}eef_data_t;

eef_data_t *eef_data = (eef_data_t*)EEFLASH_ADDRESS;
#define PAGE_SIZE	(4096)
#define NPAGES		(EEFLASH_SIZE / PAGE_SIZE)
#define NRECS		(PAGE_SIZE / sizeof(eef_data_t))
#define EEF_TOTAL	(NPAGES * NRECS)

static eef_data_t* eef_find_last(){
  eef_data_t *res = (eef_data_t*)EEFLASH_ADDRESS;
  uint32_t nres = 0;
  eef_data_t *cur;
  uint32_t ncur;
  for(int i=0; i<NPAGES; i++){
    cur = (eef_data_t*)(EEFLASH_ADDRESS + (i*PAGE_SIZE));
    for(int j=0; j<NRECS; j++){
      ncur = cur[j].wnum ^ MEM_XOR32;
      if(ncur > nres){
        nres = ncur; res = &cur[j];
      }
    }
  }
  return res;
}

void eeflash_read(void *data){
  for(int i=0; i<sizeof(eef_data->data)/sizeof(uint16_t); i++){
    union{
      uint16_t v16;
      uint8_t v8[2];
    }temp;
    temp.v16 = eef_data->data[i] ^ MEM_XOR;
    ((uint8_t*)data)[i*2+0] = temp.v8[0];
    ((uint8_t*)data)[i*2+1] = temp.v8[1];
  }
}

void eeflash_write(void *data){
  //Check if data is equal with previous
  char eq_flag = 1;
  for(int i=0; i<sizeof(eef_data->data)/sizeof(uint16_t); i++){
    union{
      uint16_t v16;
      uint8_t v8[2];
    }temp;
    temp.v8[0] = ((uint8_t*)data)[i*2 + 0];
    temp.v8[1] = ((uint8_t*)data)[i*2 + 1];
    temp.v16 ^= MEM_XOR;
    if(eef_data->data[i] != temp.v16){eq_flag = 0; break;}
  }
  if(eq_flag)return;
  
  //Change F_CPU to 8MHz - safety speed for writiing flash
  uint32_t speed_prev = (RCC->CFGR0 & RCC_SW);
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | RCC_SW_HSI;
  while((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI){}
  
  flash_unlock();
  
  uint32_t wnum = eef_data->wnum ^ MEM_XOR32;
  wnum++;
  
  uint32_t i = (uint32_t)eef_data - EEFLASH_ADDRESS;
  uint32_t np = i / PAGE_SIZE;
  i %= PAGE_SIZE;
  
  i+=sizeof(eef_data_t);
  if(i >= (PAGE_SIZE-sizeof(eef_data_t))){
    i = 0;
    np++;
    if(np >= NPAGES)np = 0;
    eef_data = (eef_data_t*)( EEFLASH_ADDRESS + np*PAGE_SIZE + i );
    //erase page
    flash_erase_4K( (uint32_t)eef_data );
  }else{
    eef_data++;
  }
  
  flash_w32( (uint32_t)&(eef_data->wnum), wnum ^ MEM_XOR32 );
  
  for(int i=0; i<sizeof(eef_data->data)/sizeof(uint16_t); i++){
    union{
      uint16_t v16;
      uint8_t v8[2];
    }temp;
    temp.v8[0] = ((uint8_t*)data)[i*2 + 0];
    temp.v8[1] = ((uint8_t*)data)[i*2 + 1];
    
    temp.v16 ^= MEM_XOR;
    flash_w16( (uint32_t)&(eef_data->data[i]), temp.v16);
  }
  
  flash_lock();
  
  //restore clock
  uint32_t resbit = 0;
  if(speed_prev == RCC_SW_HSI)resbit = RCC_SWS_HSI;
  if(speed_prev == RCC_SW_HSE)resbit = RCC_SWS_HSE;
  if(speed_prev == RCC_SW_PLL)resbit = RCC_SWS_PLL;
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | speed_prev;
  while((RCC->CFGR0 & RCC_SWS) != resbit){}
}

void eeflash_erase(){
  //Change F_CPU to 8MHz - safety speed for writiing flash
  uint32_t speed_prev = (RCC->CFGR0 & RCC_SW);
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | RCC_SW_HSI;
  while((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI){}
  
  flash_unlock();
  
  uint32_t addr;
  for(int i=0; i<NPAGES; i++){
    addr = EEFLASH_ADDRESS + (i*PAGE_SIZE);
    flash_erase_4K(addr);
  }
  
  
  flash_lock();
  
  //restore clock
  uint32_t resbit = 0;
  if(speed_prev == RCC_SW_HSI)resbit = RCC_SWS_HSI;
  if(speed_prev == RCC_SW_HSE)resbit = RCC_SWS_HSE;
  if(speed_prev == RCC_SW_PLL)resbit = RCC_SWS_PLL;
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | speed_prev;
  while((RCC->CFGR0 & RCC_SWS) != resbit){}
}

void eeflash_init(){
  eef_data = eef_find_last();
}

#undef MEM_XOR
#undef MEM_XOR32
#undef PAGE_SIZE
#undef NPAGES
#undef NRECS
#undef EEF_TOTAL

/*
void eef_dump_idx(uint32_t i){
  uint32_t np = i / NRECS;
  i = i % NRECS;
  eef_data_t *cur = (eef_data_t*)(EEFLASH_ADDRESS + (np*PAGE_SIZE) + i*sizeof(eef_data_t));
  
  uint32_t wnum = cur->wnum ^ MEM_XOR32;
  while(UART_avaible(USART) < 100){}
  UART_puts(USART, fpi32tos(NULL, np, 0, 0));
  UART_puts(USART, ".");
  UART_puts(USART, fpi32tos(NULL, i, 0, 0));
  UART_puts(USART, "(");
  UART_puts(USART, fpi32tos(NULL, wnum, 0, 0));
  UART_puts(USART, "){ ");
  for(int i=0; i<sizeof(eef_data->data)/sizeof(uint16_t); i++){
    uint16_t dat = cur->data[i] ^ MEM_XOR;
    while(UART_avaible(USART) < 100){}
    UART_puts(USART, u32tohex(NULL, dat, 4));
    UART_puts(USART, " ");
  }
  UART_puts(USART, "}\r\n");
}

void eef_print_idx(eef_data_t *pos){
  uint32_t i = (uint32_t)pos - EEFLASH_ADDRESS;
  uint32_t np = i / PAGE_SIZE;
  i %= PAGE_SIZE;
  i /= sizeof(eef_data_t);
  while(UART_avaible(USART)<100){}
  UART_puts(USART, u32tohex(NULL, pos, 8));
  UART_puts(USART, "=");
  UART_puts(USART, fpi32tos(NULL, np, 0, 0));
  UART_puts(USART, ".");
  UART_puts(USART, fpi32tos(NULL, i, 0, 0));
}
*/

#endif