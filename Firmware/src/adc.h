#ifndef __ADC_H__
#define __ADC_H__

#if 1==0

#include "adc.h"

ADC_init(ADC_1, ADC_TS_VREF_EN, 6);
ADC_SAMPLING_TIME( ADC_1, marg5(ADC_x1), ADC_SAMPL_TIME_2 );
ADC_SEQ_SET( ADC_1, 1, marg5(ADC_x1) );
ADC_SEQ_CNT( ADC_1, 1 );
ADC_Trigger( ADC_1, ADC_TRIG_DISABLE );

#endif

#include "pinmacro.h"

//only on ADC1 master avaible
#define ADC_CH_VREF 17 //внутренний ИОН ~1.2 В
#define ADC_CH_TEMP 16 //внутренний датчик температуры, (Tsample >= 17.1 us)

#define ADC_1	1
#define ADC_2	2

#define _ADC(x)	ADC##x
#define ADC(x) _ADC(x)

//conv_time = 12.5
#define ADC_SAMPL_TIME_2	0 //1.5		-> 14
#define ADC_SAMPL_TIME_8	1 //7.5		-> 20
#define ADC_SAMPL_TIME_14	2 //13.5	-> 26
#define ADC_SAMPL_TIME_29	3 //28.5	-> 41
#define ADC_SAMPL_TIME_42	4 //41.5	-> 54
#define ADC_SAMPL_TIME_56	5 //55.5	-> 68
#define ADC_SAMPL_TIME_72	6 //71.5	-> 84
#define ADC_SAMPL_TIME_240	7 //239.5	-> 252

#define ADC_TRIG_TIM1_1	((0*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_TIM1_2	((1*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_TIM1_3	((2*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_TIM2_2	((3*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_TIM3TR	((4*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_TIM4_4	((5*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_EXTI11_TIM8TR	((6*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_SW		((7*ADC_EXTSEL_0) | ADC_EXTTRIG)
#define ADC_TRIG_DISABLE (0)

#define ADC1_DMA	1,1,do{ADC1->CTLR2 |= ADC_DMA;}while(0),do{ADC1->CTLR2 &=~ ADC_DMA;}while(0)



#define ADCSQR_1  3
#define ADCSQR_2  3
#define ADCSQR_3  3
#define ADCSQR_4  3
#define ADCSQR_5  3
#define ADCSQR_6  3

#define ADCSQR_7  2
#define ADCSQR_8  2
#define ADCSQR_9  2
#define ADCSQR_10 2
#define ADCSQR_11 2
#define ADCSQR_12 2

#define ADCSQR_13 1
#define ADCSQR_14 1
#define ADCSQR_15 1
#define ADCSQR_16 1

#define __ADC_RSQR(adc, sqr) ADC##adc ->RSQR##sqr
#define _ADC_RSQR(adc, sqr) __ADC_RSQR(adc, sqr)
#define ADC_RSQR(adc, num) _ADC_RSQR(adc, ADCSQR_##num)

#define _ADC_SQR_SQ(num) ADC_SQ ## num
#define ADC_SQR_SQ(num) _ADC_SQR_SQ(num)

#define ADC_SEQ_SET(adc, elem, val) do{\
  PM_BITMASK( ADC_RSQR(adc, elem), ADC_SQR_SQ(elem), val );\
}while(0)

#define _ADC_SEQ_CNT(adc, cnt) PM_BITMASK( ADC##adc ->RSQR1, ADC_L, (cnt) )
#define ADC_SEQ_CNT(adc, cnt) _ADC_SEQ_CNT(adc, (cnt)-1)

//SMPR
#define ADCSMPR_0  2
#define ADCSMPR_1  2
#define ADCSMPR_2  2
#define ADCSMPR_3  2
#define ADCSMPR_4  2
#define ADCSMPR_5  2
#define ADCSMPR_6  2
#define ADCSMPR_7  2
#define ADCSMPR_8  2
#define ADCSMPR_9  2

#define ADCSMPR_10 1
#define ADCSMPR_11 1
#define ADCSMPR_12 1
#define ADCSMPR_13 1
#define ADCSMPR_14 1
#define ADCSMPR_15 1
#define ADCSMPR_16 1
#define ADCSMPR_17 1

#define __ADC_SMPR(adc, smp) ADC##adc->SAMPTR##smp
#define _ADC_SMPR(adc, smp) __ADC_SMPR(adc, smp)
#define ADC_SMPR(adc, num) _ADC_SMPR(adc, ADCSMPR_##num)

#define __ADC_SMPR_SMP(smp, num) ADC_SMP ## num
#define _ADC_SMPR_SMP(smp, num) __ADC_SMPR_SMP(smp, num)
#define ADC_SMPR_SMP(num) _ADC_SMPR_SMP(ADCSMPR_##num, num)

#define ADC_SAMPLING_TIME(adc, chan, time) PM_BITMASK( ADC_SMPR(adc, chan), ADC_SMPR_SMP(chan), time )

//RCC_CFGR
#define ADCPRE2 (0 << RCC_CFGR_ADCPRE_Pos)
#define ADCPRE4 (1 << RCC_CFGR_ADCPRE_Pos)
#define ADCPRE6 (2 << RCC_CFGR_ADCPRE_Pos)
#define ADCPRE8 (3 << RCC_CFGR_ADCPRE_Pos)
#define _ADCPRE(x) ADCPRE##x
#define ADCPRE(x) _ADCPRE(x)

#define ADC_Trigger( num, trigger ) do{ \
    PM_BITMASK(ADC(num)->CTLR2, ADC_EXTSEL, (trigger &~ ADC_EXTTRIG)); \
    if(trigger & ADC_EXTTRIG) ADC(num)->CTLR2 |= ADC_EXTTRIG; else ADC(num)->CTLR2 &=~ ADC_EXTTRIG; \
  }while(0)

#define ADC_ENR(num) RCC_ADC ## num ## EN
#define RCC_ADC_DIV(n) RCC_ADCPRE_DIV ## n

#define ADC_TS_VREF_EN	1
#define ADC_TS_VREF_DIS	0
  
//div = 2, 4, 6, 8. F_ADC = F_APB2 / div; F_ADC must be < 14 MHz
#define ADC_init( num, vrefen, div ) do{ \
  RCC->APB2PCENR |= ADC_ENR(num); \
  ADC(num)->CTLR2 |= ADC_ADON | (vrefen * ADC_TSVREFE); \
  RCC->CFGR0 = (RCC->CFGR0 &~ RCC_ADCPRE) | RCC_ADC_DIV(div); \
  ADC_CALIBRATE( num ); \
}while(0)

#define ADC_vref_mode(num, vrefen) do{\
  if(vrefen)ADC(num)->CTLR2 |= ADC_TSVREFE; else ADC(num)->CTLR2 &=~ADC_TSVREFE; \
}while(0)

#define ADC_CALIBRATE(num) do{ \
    for(uint32_t i=0; i<200; i++)asm volatile("nop"); \
    ADC(num)->CTLR2 |= ADC_CAL; \
    while(ADC(num)->CTLR2 & ADC_CAL){} \
  }while(0)
  
#define ADC_start(num) do{ADC(num)->CTLR2 |= ADC_ADON;}while(0)

#define ADC_SCAN_1CH	0
#define ADC_SCAN_SEQ	1
#define ADC_SCAN_SINGLE	0
#define ADC_SCAN_CYCLE	1  
#define ADC_SCANMODE(num, scan, cycle) do{\
    PM_BITMASK(ADC(num)->CTLR1, ADC_SCAN, scan); \
    PM_BITMASK(ADC(num)->CTLR2, ADC_CONT, cycle); \
  }while(0)
  
#define ADC_data(num)	(ADC(num)->RDATAR)
  
static uint16_t adc1_read(int ch){
  ADC_SEQ_SET( ADC_1, 1, ch );
  ADC1->CTLR2 |= ADC_ADON;
  while(! (ADC1->STATR & ADC_EOC )){}
  return ADC1->RDATAR;
}

#endif