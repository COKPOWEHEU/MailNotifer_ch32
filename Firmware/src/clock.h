#ifndef __V203_CLOCK_H__
#define __V203_CLOCK_H__

#ifndef CH32V203G8
  #error This clock.h specified for ch32v203g8 only
#endif

#define CLOCK_HSI 0
#define CLOCK_HSE 1

static int8_t clock_HS(int8_t quartz){
  uint32_t timeout;
  RCC->CFGR0 |= RCC_HPRE_DIV1;
  RCC->CFGR0 |= RCC_PPRE2_DIV2;
  RCC->CFGR0 |= RCC_PPRE1_DIV2;
  RCC->CFGR0 &=~(RCC_PLLSRC | RCC_PLLXTPRE | RCC_PLLMULL);

  EXTEN->EXTEN_CTR |= EXTEN_PLL_HSI_PRE;
  RCC->CFGR0 |= RCC_PLLSRC_HSI_Div2 | RCC_PLLMULL18;
  
  RCC->CTLR |= RCC_PLLON;
  while(!(RCC->CTLR & RCC_PLLRDY)){}
  
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_SW) | RCC_SW_PLL;
  while((RCC->CFGR0 & RCC_SWS) != RCC_SWS_PLL){}
  
  //USB prescaler: 00=PLLMUL/1, 01=PLLMUL/2, 10=PLLMUL/3
  RCC->CFGR0 = (RCC->CFGR0 &~(0b11*RCC_USBPRE)) | (0b10*RCC_USBPRE);
  
  return CLOCK_HSI;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//  SysTick ///////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#define SysTick_CTLR_STE (1<<0) //systick enable
#define SysTick_CTLR_STIE (1<<1) //interrupt enable
#define SysTick_CTLR_STCLK (1<<2) //clock source: 0-HCLK/8, 1-HCLK/1
#define SysTick_CTLR_STRE (1<<3) //auto reload enable
#define SysTick_CTLR_MODE (1<<4) //up/down. 0-up, 1-down
#define SysTick_CTLR_INIT (1<<31) //1: Updated to 0 when upcounting, while updated to the comparison value when downcounting.

#define SysTick_SR_CNTIF (1<<0) //interrupt flag


static inline void systick_init(){
  SysTick->SR=0;
  SysTick->CNT=0;
  
  SysTick->CTLR = SysTick_CTLR_STCLK | SysTick_CTLR_STE;
}

static inline int32_t systick_read32(){
  //return SysTick->CNT;
  return ((volatile int32_t*)&(SysTick->CNT))[0]; //Спасибо наркоманам из WCH, которые не дают доступа к CNTH:CNTL
}

static inline uint64_t systick_read64(){
  return SysTick->CNT;
}

static void delay_ticks(int32_t t){
  int32_t t_prev = systick_read32();
  while( (systick_read32() - t_prev) < t ){}
}

#define _PB_MAKE_NAME(x,y) PERIOD_BLOCK_VARIABLE_ ## x ## y
#define PB_MAKE_NAME(x,y) _PB_MAKE_NAME(x, y)

#define PB_av	PB_MAKE_NAME(__LINE__, _av)
#define PB_cur	PB_MAKE_NAME(__LINE__, _cur)

#define PERIOD_BLOCK(t_cur, period) \
  static uint32_t PB_av = 0; \
  uint32_t PB_cur = t_cur; \
  if( ((PB_av - PB_cur) & (1<<31)) && (PB_av = PB_cur + (period))   )
    
#define PERIOD_BLOCK_REV(t_cur, period) \
  static uint32_t PB_av = 0; \
  uint32_t PB_cur = t_cur; \
  if( (PB_av - PB_cur) & (1<<31) ){ PB_av = PB_cur + (period); }else
    
    

#define __TIME_S__ ((__TIME__[0]-'0')*36000+(__TIME__[1]-'0')*3600 + (__TIME__[3]-'0')*600+(__TIME__[4]-'0')*60 + (__TIME__[6]-'0')*10+(__TIME__[7]-'0'))
#define __DATE_D__ (((__DATE__[4]==' '?'0':__DATE__[4])-'0')*10 + (__DATE__[5]-'0'))
#define __DATE_M__ (((__DATE__[1]=='a')&&(__DATE__[2]=='n'))?1: \
                    (__DATE__[2]=='b')?2: \
                    ((__DATE__[1]=='a')&&(__DATE__[2]=='r'))?3: \
                    ((__DATE__[1]=='p')&&(__DATE__[2]=='r'))?4: \
                    (__DATE__[2]=='y')?5: \
                    ((__DATE__[1]=='u')&&(__DATE__[2]=='n'))?6: \
                    (__DATE__[2]=='l')?7: \
                    (__DATE__[2]=='g')?8: \
                    (__DATE__[2]=='p')?9: \
                    (__DATE__[2]=='t')?10: \
                    (__DATE__[2]=='v')?11: \
                    (__DATE__[2]=='c')?12: \
                   0)
#define __DATE_Y__ ((__DATE__[7]-'0')*1000 + (__DATE__[8]-'0')*100 + (__DATE__[9]-'0')*10 + (__DATE__[10]-'0'))
#ifndef DATETIME_TIMEZONE_S
  #define DATETIME_TIMEZONE_S	(3*3600)
#endif
#define _DATETIME_UTCOFFSET 951868800
//При расчете продолжительности года удобнее перенести февраль в конец года.
//Тогда длительность до текущего месяца вычисляется из массива
//const uint16_t day_offset[12] = {0, 31, 61, 92, 122, 153, 184, 214, 245, 275,306, 337};
//но в макрос нельзя встроить массив, поэтому длительность будем вычислять через кучу if-ов (снова от января)
#define _DAY_OFFSET(m) ((m)==0?275:(m)==1?306:(m)==2?337:(m)==3?0:(m)==4?31:(m)==5?61:(m)==6?92:(m)==7?122:(m)==8?153:(m)==9?184:(m)==10?214:245)
//длительность текущего года (в днях) с учетом високосности
#define __DATETIME_YEAR_DAYS(y) ((y)*365 + (y)/4 - (y)/100 + (y)/400)
//корректировка на начало года с марта
#define _DATETIME_YEAR_DAYS(m, y) __DATETIME_YEAR_DAYS((y)-2000-((m)<3))
#define UTC(h,m,s,dd,mm,yyyy) (((dd) - 1 + _DAY_OFFSET(mm) + _DATETIME_YEAR_DAYS(mm, yyyy)) * 86400 + (h)*3600 + (m)*60 + (s) + _DATETIME_UTCOFFSET - DATETIME_TIMEZONE_S)
#define __UTC__ UTC(0,0,__TIME_S__, __DATE_D__, __DATE_M__, __DATE_Y__)

#endif
