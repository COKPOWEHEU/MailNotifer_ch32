#ifndef __SAVEDATA_H__
#define __SAVEDATA_H__

#include "lcd_hd44780.h"
typedef struct{
  struct{
    uint32_t contrast;
    uint8_t backlight;
    lcd_cur_t cursor;
    recode_t recode_table[64];
    newline_mode_t newline_mode;
    usersym_t usersym[8];
  }lcd;
  struct{
    uint8_t colortable[8];
    uint8_t colorpwm[3];
  }rgb;
  struct{
    uint32_t freq_Hz;
    uint32_t dur_ms;
    uint32_t vol; //0 - 100
  }snd;
}device_settings_t;

extern device_settings_t device_settings;

#endif