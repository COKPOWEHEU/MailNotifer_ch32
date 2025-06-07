#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#define RLED	B,0,1,GPIO_APP50
#define RTIM	3,3,TIMO_PWM_NINV
#define GLED	A,6,1,GPIO_APP50
#define GTIM	3,1,TIMO_PWM_NINV
#define BLED	A,7,1,GPIO_APP50
#define BTIM	3,2,TIMO_PWM_NINV

#define LCD_D4	B,12,1,GPIO_PP50
#define LCD_D5	B,13,1,GPIO_PP50
#define LCD_D6	B,14,1,GPIO_PP50
#define LCD_D7	B,15,1,GPIO_PP50
#define LCD_RS	B,10,1,GPIO_PP50
#define LCD_E	B,11,1,GPIO_PP50
#define LCD_BL			A,8,1,GPIO_APP50
#define LCD_BL_TIM		1,1,TIMO_PWM_NINV | TIMO_POS, 255 // max PWM=255
#define LCD_CONT		A,1,1,GPIO_APP50, 3600 //max ADC=3600
#define LCD_CONT_TIM	2,2,TIMO_PWM_NINV, 255
#define LCD_CONT_ADC	A,5,1,GPIO_ADC,5 // 4.84V = 3530adc
#define LCD_W			16
#define LCD_H			2
#define LCD_F_CPU		144000000

#define BTN		A,4,0,GPIO_PULL

#define SND		A,9,1,GPIO_APP50
#define TSND	1,2,TIMO_PWM_NINV | TIMO_POS

#define TSND2	3,1


#include "pinmacro.h"
  
//------ USB -------------------
//PMA_size = 512
//EP0_size = 8

// HID
#define ENDP_HID		1
#define ENDP_HID_SIZE	8
// /dev/ttyACM0 - TTY
#define ENDP_TTY_IN		2
#define ENDP_TTY_OUT	2
#define ENDP_TTY_SIZE	32 //->64
#define ENDP_TTY_CTL	3
#define ENDP_CTL_SIZE	8
//MSD
#define ENDP_MSD_IN		4
#define ENDP_MSD_OUT	4
#define ENDP_MSD_SIZE	64 //->128

#define interface_hid	0, 1
#define interface_tty	1, 2
#define interface_msd	3, 1

#define interface_count	3 //HID + TTY
#define interface_count_alt	4//HID + TTY + MSD
extern volatile uint8_t usb_class_mode;

//#define interface_count	1


#define ifnum(x) _marg1(x)
#define ifcnt(x) _marg2(x)

#endif
