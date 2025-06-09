#ifndef __LCD_HD44780_H__
#define __LCD_HD44780_H__

#include <stdint.h>
#include "hardware.h"

typedef enum{
  CUR_NONE = 0,
  CUR_LARGE = 1,
  CUR_SMALL = 2,
  CUR_BOTH = 3
}lcd_cur_t;

typedef struct{
  uint32_t sym;
  uint8_t disp_as_hex;
  uint8_t code;
}recode_t;

typedef struct{
  uint32_t sym;
  uint8_t data[8];
}usersym_t;

typedef enum{
  NL_NORMAL = 0,	// /n=newline (x=0; y++) ; /r=return to line start (x=0;)
  NL_NEWLINE =1,	// /r = /n = newline (x=0; y++); /r/n = one newline
  NL_IGNORE = 2,	// /r, /n = do nothing
}newline_mode_t;

extern void (*lcd_beep_func)(void);
extern char *lcd_temptext;

void lcd_init();
void lcd_bl(uint8_t val);
void lcd_cont(uint32_t cont);
void lcd_update(uint32_t time, uint32_t cur_adc);
void lcd_putc(char ch);
void lcd_puts(char *str);

//////////////////////////////////////////////////////////////
#define LCD_HELP1 \
  "Этот модуль отображает принятые по COM-порту символы на дисплее\r\n" \
  "\r\n" \
  "Аппаратно поддерживаются следующие символы:\r\n" \
  "   !@#$%&'()*+,-./0123456789:;<=>?@[\\]^_`\r\n" \
  "  ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n" \
  "  abcdefghijklmnopqrstuvwxyz\r\n" \
  "  АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ\r\n" \
  "  абвгдеёжзийклмнопрстуфхцчшщъыьэюяѐ\r\n" \
  "  ~£§«°¶¼½¾¿çĳƒЁ“„⅓ⅠⅡ↑↓↵⇤⇥⒑⒓⒖█\r\n" \
  "Также возможно задать до 8 пользовательских символов. Сейчас они таковы:\r\n  "
  
#define LCD_HELP2 "𒀀𒀀𒀀𒀀𒀀𒀀𒀀𒀀  \r\n" //user symbols
  
#define LCD_HELP3 \
  "\r\n" \
  "Для более гибкого управления отображением поддерживаются следующие ESC-последовательности:\r\n" \
  "  \\e[yH    - переместить курсор в (1, y)\n" \
  "  \\e[y;xH  - переместить курсор в (x, y)\n" \
  "  \\e[J     - Очистить дисплей и переместить курсор в (1,1)\n" \
  "  \\e[1 q   - Отображение курсора в виде мигающего блока\n" \
  "  \\e[4 q   - Отображение курсора в виде подчеркивания\n" \
  "  \\e[7 q   - Отображение курсора одновременно в виде блока и подчеркивания\n" \
  "  \\e[?25l  - Отображение курсора отключено\n" \
  "Управление реакцией на \\r, \\n:\r\n" \
  "  \\e[0.    - \\r - возврат каретки, \\n - перевод строки\r\n" \
  "  \\e[1.    - \\r, \\n оба - перевод строки (пара \\r\\n считается одним переводом строки)\r\n" \
  "  \\e[2.    - \\r, \\n игнорируются (используйте \\e[y;xH\r\n" \
  "Прочие символы:\r\n" \
  "  \\b       - Пищалка\r\n" \
  "  \\t       - tab (выравнивание на 8 символов)\r\n" \
  "  \\n, \\r   - перевод строки (см. выше)\r\n"

#define LCD_HELP_SZ (sizeof(LCD_HELP1) + sizeof(LCD_HELP2) + sizeof(LCD_HELP3) )

void vf_lcdcfg_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);
void vf_lcdcfg_write(uint8_t *buf, uint32_t addr, uint16_t file_idx);

void vf_usersym_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);
void vf_usersym_write(uint8_t *buf, uint32_t addr, uint16_t file_idx);

void vf_usertbl_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);
void vf_usertbl_write(uint8_t *buf, uint32_t addr, uint16_t file_idx);

void vf_lcdhelp_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);

#include "usb_class_virfat.h"
#define lcd_cfg_virfat_files \
{ \
  .name = "LCD_CFG CFG", \
  .file_read = vf_lcdcfg_read, \
  .file_write = vf_lcdcfg_write, \
  .size = 1, \
}, \
{ \
  .name = "LCD_HELPTXT", \
  .file_read = vf_lcdhelp_read, \
  .file_write = virfat_file_dummy, \
  .size = (LCD_HELP_SZ + 511) / 512, \
}, \
{ \
  .name = "USRSYM_0CFG", \
  .userdata = (void*)0, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_1CFG", \
  .userdata = (void*)1, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_2CFG", \
  .userdata = (void*)2, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_3CFG", \
  .userdata = (void*)3, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_4CFG", \
  .userdata = (void*)4, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_5CFG", \
  .userdata = (void*)5, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_6CFG", \
  .userdata = (void*)6, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRSYM_7CFG", \
  .userdata = (void*)7, \
  .file_read = vf_usersym_read, \
  .file_write = vf_usersym_write, \
  .size = 1, \
}, \
{ \
  .name = "USRTBL_1CFG", \
  .userdata = (void*)0, \
  .file_read = vf_usertbl_read, \
  .file_write = vf_usertbl_write, \
  .size = 1, \
}, \
{ \
  .name = "USRTBL_2CFG", \
  .userdata = (void*)1, \
  .file_read = vf_usertbl_read, \
  .file_write = vf_usertbl_write, \
  .size = 1, \
}, \
{ \
  .name = "USRTBL_3CFG", \
  .userdata = (void*)2, \
  .file_read = vf_usertbl_read, \
  .file_write = vf_usertbl_write, \
  .size = 1, \
}, \
{ \
  .name = "USRTBL_4CFG", \
  .userdata = (void*)3, \
  .file_read = vf_usertbl_read, \
  .file_write = vf_usertbl_write, \
  .size = 1, \
}, \


#endif

// 0xxxxxxx                              | 0 - 127
// 110xxxxx 10xxxxxx                     | 192 - 223 | 128 - 191
// 1110xxxx 10xxxxxx 10xxxxxx            | 224 - 239
// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx   | 240 - 247

// 2 / 2 | 0x3000 - 37FF
// 3 / 2 | 0x3800 - 3BFF
// 3 / 3 | 0xE'0000 - E'FFFF
// 4 / 2 | 0x3C00 - 3DFF
// 4 / 3 | 0xF'0000 - F'7FFF
// 4 / 4 | 0x3C0'0000 - 3DF'FFFF

//QW 
//ФЫ 0424 042B
//൦╔ 000D66 002554
//𒀀𝄞 00012000 0001D11E