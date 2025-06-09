#include "pinmacro.h"
#include "ch32v20x.h"
#include "lcd_hd44780.h"
#include "timer.h"
#include "clock.h"
#include "strlib.h"
//#define UART_DECLARATIONS 2
//#include "uart.h"

#if !defined(LCD_D4) || !defined(LCD_D5) || !defined(LCD_D6) || !defined(LCD_D7) || !defined(LCD_RS) || !defined(LCD_E)
  #error define LCD_D4, LCD_D5, LCD_D6, LCD_D7, LCD_RS, LCD_E
#endif

#if !defined(LCD_BL) || !defined(LCD_BL_TIM)
  #error define LCD_BL and LCD_BL_TIM
#endif

#if !defined(LCD_CONT) || !defined(LCD_CONT_TIM)
  #error define LCD_CONT and LCD_CONT_TIM
#endif

#warning LCD_BL_TIM, LCD_CONT_TIM and LCD_CONT_ADC are external. This library does not init them.


#define LCD_100us	(LCD_F_CPU / 10000)
#define lcd_delay_us(t_us) delay_ticks( LCD_100us * t_us / 100 )

#if LCD_H==4
	#define LCD_STR_1	0x00
	#define LCD_STR_2	0x40
	#define LCD_STR_3	0x10
	#define LCD_STR_4	0x50
	const uint8_t lcd_str_pos[] = {LCD_STR_1, LCD_STR_2, LCD_STR_3, LCD_STR_4};
#endif
#if LCD_H==2
	#define LCD_STR_1	0x00
	#define LCD_STR_2	0x40
    const uint8_t lcd_str_pos[] = {LCD_STR_1, LCD_STR_2};
#endif

char lcd_buffer[LCD_W * LCD_H];
static uint8_t lcd_cur_x = 0, lcd_cur_y = 0;
static char buf_update_flag = 0;
static char usersym_update = 0;
static char save_flag = 0;
void (*lcd_beep_func)(void) = NULL;
char *lcd_temptext = NULL;

#include "savedata.h"
#define lcd_cont_val	(device_settings.lcd.contrast)
#define lcd_bl_val		(device_settings.lcd.backlight)
#define lcd_cur			(device_settings.lcd.cursor)
#define recode_dynamic	(device_settings.lcd.recode_table)
#define newline_mode	(device_settings.lcd.newline_mode)
#define usersym			(device_settings.lcd.usersym)
/*
#warning TODO: FLASH
static uint32_t lcd_cont_val = 0; //TODO: FLASH
static uint8_t lcd_bl_val = 10; //TODO: FLASH
lcd_cur_t lcd_cur = CUR_NONE; //TODO: FLASH
recode_t recode_dynamic[64]; //TODO: FLASH
newline_mode_t newline_mode = NL_NEWLINE; //TODO: FLASH
usersym_t usersym[8] = { //TODO: FLASH
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
  {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}},
};
*/
//| ### |
//|## ##|
//|# # #|
//|### #|
//|## ##|
//|#####|
//|## ##|
//| ### |

#define DATA_START marg2(LCD_D4)
#define DATA_MASK (0b1111 << DATA_START << 16)

static void lcd_send(unsigned char data){
  lcd_delay_us(300);
  GPO_ON(LCD_E);
  lcd_delay_us(500);
  GPIO(marg1(LCD_D7))->BSHR = DATA_MASK | ((data>>4)<<DATA_START);
  GPO_OFF(LCD_E);
  lcd_delay_us(500);
  GPO_ON(LCD_E);
  GPIO(marg1(LCD_D7))->BSHR = DATA_MASK | ((data&0xF)<<DATA_START);
  GPO_OFF(LCD_E);
  lcd_delay_us(500);
}

static void lcd_cmd(char cmd){GPO_OFF(LCD_RS); lcd_send(cmd);}
static void lcd_data(char data){GPO_ON(LCD_RS); lcd_send(data);}

void lcd_init(){
  TIMER_CLOCK(marg1(LCD_BL_TIM), 1);
  
  GPIO_config(LCD_BL); GPIO_config(LCD_CONT);
  GPIO_config(LCD_D4); GPIO_config(LCD_D5); GPIO_config(LCD_D6); GPIO_config(LCD_D7);
  GPIO_config(LCD_RS); GPIO_config(LCD_E);
  timer_chval(LCD_BL_TIM) = 0; timer_chcfg(LCD_BL_TIM); timer_chpol(LCD_BL_TIM, LCD_BL);
  timer_chval(LCD_CONT_TIM) = 0; timer_chcfg(LCD_CONT_TIM); timer_chpol(LCD_CONT_TIM, LCD_CONT);
  lcd_bl(lcd_bl_val);
  lcd_cont(80);
// LCD init sequence (some magick inside)
  lcd_delay_us(1000);
  GPO_OFF(LCD_RS);
  GPO_ON(LCD_E);
  lcd_delay_us(1000);
  // send 0011
  GPO_OFF(LCD_D7); GPO_OFF(LCD_D6); GPO_ON(LCD_D5); GPO_ON(LCD_D4);
  GPO_OFF(LCD_E);
  lcd_delay_us(5000); //lcd_delay_us(10000); // > 4.1 ms
  GPO_ON(LCD_E);
  lcd_delay_us(1000);
  GPO_OFF(LCD_E);
  lcd_delay_us(1000);
  GPO_ON(LCD_E);
  lcd_delay_us(1000); //lcd_delay_us(10000); // > 100 us
  GPO_OFF(LCD_E);
  lcd_delay_us(1000);
  //send 0010
  GPO_OFF(LCD_D4);
  GPO_ON(LCD_E);
  lcd_delay_us(10000);
  GPO_OFF(LCD_E);
  
  lcd_send(0b00101000); // bus config: 4-bit, 2-lines, 5x8
  lcd_send(0b00000110); // cursor increment: pos++, no screen shift

  lcd_send(0b00001100); // display mode: display ON, cursor OFF
  lcd_send(0b00000010); // reset cursor position
  lcd_send(0b00000001); // display clear
  lcd_delay_us(10000);
  
  memset(lcd_buffer, ' ', sizeof(lcd_buffer));
  //recode_dynamic
  for(int i=0; i<(sizeof(recode_dynamic)/sizeof(recode_dynamic[0])); i++){
    recode_dynamic[i].sym = U'�';
    recode_dynamic[i].disp_as_hex = (i&1);
    recode_dynamic[i].code = 0xF8;
  }
  /*
  recode_dynamic[1].sym = L'A';
  recode_dynamic[3].sym = L'Ф';
  recode_dynamic[5].sym = L'൦';
  recode_dynamic[7].sym = L'𒀀';
  //*/
  const usersym_t usym = {U'�', {0x0E, 0x1B, 0x15, 0x1D, 0x1B, 0x1F, 0x1B, 0x0E}};
  for(int i=0; i<8; i++){
    memcpy((void*)&usersym[i], (void*)&usym, sizeof(usym));
  }
}

void lcd_bl(uint8_t val){
  lcd_bl_val = val;
  timer_chval(LCD_BL_TIM) = ((uint32_t)val * marg4(LCD_BL_TIM) + 50) / 100;
}

void lcd_cont(uint32_t cont){
  lcd_cont_val = ((100 - cont) * 4096 + 50) / 100;
}

enum{
  BP_START = 100,
  BP_CUR_HIDE,
  BP_RESET,
  BP_GOTO_1,
  //send data = 0...LCD_W-1
  BP_GOTO_2,
  //send_data = LCD_W...2*LCD_W-1
  BP_GOTO_3,
  //send_data = 2*LCD_W...3*LCD_W-1
  BP_GOTO_4,
  //send_data = 3*LCD_W...4*LCD_W-1
  BP_GOTO_XY,
  BP_CUR_SHOW,
  BP_WAIT
};
uint8_t buf_pos = BP_WAIT;

typedef enum{
  BPS_START,//
  BPS_PREP, //wait 100 us, then LCD_E = 1
  BPS_E_UP1,//wait 300 us, then write D7:D4, then LCD_E = 0
  BPS_E_DN1,//wait 300 us, then LCD_E = 1
  BPS_E_UP2,//wait 300 us, then write D3:D0, then LCD_E = 0
  BPS_E_DN2,//wait 300 us
  BPS_DONE,
  BPS_READY, //
}buf_step_t;

buf_step_t buf_step = BPS_DONE;

void bps_update(uint32_t time){
  static char bps_data = 0;
  static uint32_t t_av = 0;
  // sub-steps
  switch(buf_step){
    case BPS_START:
      buf_step = BPS_PREP;
      t_av = time;
      return;
    case BPS_PREP:
      if( (int32_t)(time - t_av) < 0 )return;
      GPO_ON(LCD_E);
      buf_step = BPS_E_UP1;
      t_av = time + LCD_100us;
      return;
    case BPS_E_UP1:
      if( (int32_t)(time - t_av) < 0 )return;
      GPIO(marg1(LCD_D7))->BSHR = DATA_MASK | ((bps_data>>4)<<DATA_START);
      GPO_OFF(LCD_E);
      buf_step = BPS_E_DN1;
      t_av = time + LCD_100us*5;
      return;
    case BPS_E_DN1:
      if( (int32_t)(time - t_av) < 0 )return;
      GPO_ON(LCD_E);
      buf_step = BPS_E_UP2;
      t_av = time + LCD_100us*5;
      return;
    case BPS_E_UP2:
      if( (int32_t)(time - t_av) < 0 )return;
      GPIO(marg1(LCD_D7))->BSHR = DATA_MASK | ((bps_data&0xF)<<DATA_START);
      GPO_OFF(LCD_E);
      buf_step = BPS_E_DN2;
      t_av = time + LCD_100us*5;
      return;
    case BPS_E_DN2:
      if( (int32_t)(time - t_av) < 0 )return;
      buf_step = BPS_DONE;
      break;
    case BPS_DONE:
      break;
    case BPS_READY:
      return;
  }
  
  //global steps
  switch(buf_pos){
    case 0 ... (LCD_W*LCD_H):
      GPO_ON(LCD_RS);
      bps_data = lcd_buffer[buf_pos];
      buf_pos++;
      if(buf_pos == LCD_W){
        buf_pos = BP_GOTO_2;
#if LCD_W == 4
      }else if(buf_pos == 2*LCD_W){
        buf_pos = BP_GOTO_3;
      }else if(buf_pos == 3*LCD_W){
        buf_pos = BP_GOTO_4;
      }else if(buf_pos == 4*LCD_W){
        buf_pos = BP_GOTO_XY;
      }
#else
      }else if(buf_pos == 2*LCD_W){
        buf_pos = BP_GOTO_XY;
      }
#endif

      buf_step = BPS_START;
      return;
    case BP_START:
      t_av = time;
      buf_pos = BP_CUR_HIDE;
      return;
    case BP_CUR_HIDE:
      if( (int32_t)(time - t_av) < 0 )return;
      GPO_OFF(LCD_RS);
      bps_data = 0b00001100; // cur hide
      buf_step = BPS_START;
      buf_pos = BP_GOTO_1;
      return;
    /*case BP_RESET:
      GPO_OFF(LCD_RS);
      bps_data = 0b00000010; // LCD reset
      buf_step = BPS_START;
      buf_pos = BP_GOTO_1;
      return;*/
    case BP_GOTO_1:
      GPO_OFF(LCD_RS);
      bps_data = 0b10000000 | LCD_STR_1;
      buf_step = BPS_START;
      buf_pos = 0;
      return;
    case BP_GOTO_2:
      GPO_OFF(LCD_RS);
      bps_data = 0b10000000 | LCD_STR_2;
      buf_step = BPS_START;
      buf_pos = LCD_W;
      return;
#if LCD_W == 4
    case BP_GOTO_3:
      GPO_OFF(LCD_RS);
      bps_data = 0b10000000 | LCD_STR_3;
      buf_step = BPS_START;
      buf_pos = LCD_W*2;
      return;
    case BP_GOTO_4:
      GPO_OFF(LCD_RS);
      bps_data = 0b10000000 | LCD_STR_4;
      buf_step = BPS_START;
      buf_pos = LCD_W*3;
      return;
#endif
    case BP_GOTO_XY:
      GPO_OFF(LCD_RS);
      bps_data = 0b10000000 | (lcd_str_pos[lcd_cur_y] + lcd_cur_x);
      buf_step = BPS_START;
      buf_pos = BP_CUR_SHOW;
      return;
    case BP_CUR_SHOW:
      GPO_OFF(LCD_RS);
      bps_data = 0b00001100 | lcd_cur;
      buf_step = BPS_START;
      buf_pos = BP_WAIT;
      return;
    case BP_WAIT:
      buf_step = BPS_READY;
      return;
  }
}

void lcd_update(uint32_t time, uint32_t cur_adc){
  static int16_t force_time = 0;
  bps_update(time);

  PERIOD_BLOCK(time, LCD_F_CPU / 1000){
    uint16_t tim = timer_chval(LCD_CONT_TIM);
    if(cur_adc > lcd_cont_val){
      if(tim < 255)tim++;
    }else{
      if(tim > 0)tim--;
    }
    timer_chval(LCD_CONT_TIM) = tim;
  }
  
  if(lcd_temptext != NULL){
    if((buf_pos != BP_WAIT) || (buf_step != BPS_READY))return;
    lcd_cmd(0x80 | LCD_STR_1);
    for(int i=0; lcd_temptext[i]!=0; i++){
      if(lcd_temptext[i] == '\n'){lcd_cmd(0x80 | LCD_STR_2); continue;}
      lcd_data(lcd_temptext[i]);
    }
    lcd_temptext = NULL;
    force_time = -50;
  }
  
  if(usersym_update){
    if((buf_pos != BP_WAIT) || (buf_step != BPS_READY))return;
    lcd_cmd(0x40); //internal RAM
    for(int i=0; i<(sizeof(usersym)/sizeof(usersym[0])); i++){
      for(int j=0; j<sizeof(usersym[i].data); j++){
        lcd_data(usersym[i].data[j]);
      }
    }
    lcd_cmd(0x80 | LCD_STR_1);
    lcd_data('[');
    for(int i=0; i<(sizeof(usersym)/sizeof(usersym[0])); i++)lcd_data(i);
    lcd_data(']');
    usersym_update = 0;
    force_time = -200;
  }
  
  if(save_flag != 0){
    static uint32_t t_av = 0;
    static uint32_t bl_prev;
    switch(save_flag){
      case 1:
        bl_prev = timer_chval(LCD_BL_TIM);
        timer_chval(LCD_BL_TIM) = 0;
        t_av = time + LCD_F_CPU/3; 
        save_flag = 2;
        break;
      case 2:
        if( (int32_t)(time - t_av) < 0 )break;
        timer_chval(LCD_BL_TIM) = 255;
        t_av = time + LCD_F_CPU/3; 
        save_flag = 3;
        break;
      default:
        if( (int32_t)(time - t_av) < 0 )break;
        timer_chval(LCD_BL_TIM) = bl_prev;
        save_flag = 0;
        break;
    }
  }
  
  PERIOD_BLOCK(time, LCD_F_CPU/10){
    force_time++;
    if(force_time > 30){buf_update_flag = 1; force_time = 0;}
    
    if(buf_update_flag && (buf_pos == BP_WAIT) && (buf_step == BPS_READY)){
      buf_pos = BP_START;
      buf_step = BPS_DONE;
      buf_update_flag = 0;
      force_time = 0;
    }
  }
  
}

void lcd_puts(char *str){
  while(str[0] != 0){
    lcd_putc(str[0]);
    str++;
  }
}








const recode_t recode_static[] = {
  {.sym=0x7E, .code=0xE9},   // [~]
  {.sym=0xA3, .code=0xCF},   // [£]
  {.sym=0xA7, .code=0xFD},   // [§]
  {.sym=0xAB, .code=0xC8},   // [«]
  {.sym=0xB0, .code=0xDF},   // [°]
  {.sym=0xB6, .code=0xFE},   // [¶]
  {.sym=0xBC, .code=0xF0},   // [¼]
  {.sym=0xBD, .code=0xF2},   // [½]
  {.sym=0xBE, .code=0xF3},   // [¾]
  {.sym=0xBF, .code=0xCD},   // [¿]
  {.sym=0xE7, .code=0xEB},   // [ç]
  {.sym=0x133, .code=0xEC},  // [ĳ]
  {.sym=0x192, .code=0xCE},  // [ƒ]
  {.sym=0x401, .code=0xA2},  // [Ё]
  {.sym=0x201C, .code=0xCB}, // [“]
  {.sym=0x201E, .code=0xCA}, // [„]
  {.sym=0x2153, .code=0xF1}, // [⅓]
  {.sym=0x2160, .code=0xD7}, // [Ⅰ]
  {.sym=0x2161, .code=0xD8}, // [Ⅱ]
  {.sym=0x2191, .code=0xD9}, // [↑]
  {.sym=0x2193, .code=0xDA}, // [↓]
  {.sym=0x21B5, .code=0x7E}, // [↵]
  {.sym=0x21E4, .code=0xDB}, // [⇤]
  {.sym=0x21E5, .code=0xDC}, // [⇥]
  {.sym=0x2491, .code=0x7B}, // [⒑]
  {.sym=0x2493, .code=0x7C}, // [⒓]
  {.sym=0x2496, .code=0x7D}, // [⒖]
  {.sym=0x2588, .code=0xFF}, // [█]
};

const uint8_t recode_cyr[] = {
  0x41, // 410, [А]
  0xA0, // 411, [Б]
  0x42, // 412, [В]
  0xA1, // 413, [Г]
  0xE0, // 414, [Д]
  0x45, // 415, [Е]
  0xA3, // 416, [Ж]
  0xA4, // 417, [З]
  0xA5, // 418, [И]
  0xA6, // 419, [Й]
  0x4B, // 41A, [К]
  0xA7, // 41B, [Л]
  0x4D, // 41C, [М]
  0x48, // 41D, [Н]
  0x4F, // 41E, [О]
  0xA8, // 41F, [П]
  
  0x50, // 420, [Р]
  0x43, // 421, [С]
  0x54, // 422, [Т]
  0xA9, // 423, [У]
  0xAA, // 424, [Ф]
  0x58, // 425, [Х]
  0xE1, // 426, [Ц]
  0xAB, // 427, [Ч]
  0xAC, // 428, [Ш]
  0xE2, // 429, [Щ]
  0xAD, // 42A, [Ъ]
  0xAE, // 42B, [Ы]
  0x62, // 42C, [Ь]
  0xAF, // 42D, [Э]
  0xB0, // 42E, [Ю]
  0xB1, // 42F, [Я]
  
  0x61, // 430, [а]
  0xB2, // 431, [б]
  0xB3, // 432, [в]
  0xB4, // 433, [г]
  0xE3, // 434, [д]
  0x65, // 435, [е]
  0xB6, // 436, [ж]
  0xB7, // 437, [з]
  0xB8, // 438, [и]
  0xB9, // 439, [й]
  0xBA, // 43A, [к]
  0xBB, // 43B, [л]
  0xBC, // 43C, [м]
  0xBD, // 43D, [н]
  0x6F, // 43E, [о]
  0xBE, // 43F, [п]
  
  0x70, // 440, [р]
  0x63, // 441, [с]
  0xBF, // 442, [т]
  0x79, // 443, [у]
  0xE4, // 444, [ф]
  0x78, // 445, [х]
  0xE5, // 446, [ц]
  0xC0, // 447, [ч]
  0xC1, // 448, [ш]
  0xE6, // 449, [щ]
  0xC2, // 44A, [ъ]
  0xC3, // 44B, [ы]
  0xC4, // 44C, [ь]
  0xC5, // 44D, [э]
  0xC6, // 44E, [ю]
  0xC7, // 44F, [я]
  
  0xEA, // 450, [ѐ]
  0xB5, // 451, [ё]
};

static void do_newline(){
  if(lcd_cur_y < LCD_H-1){
    lcd_cur_y++;
    lcd_cur_x = 0;
  }else{
    for(int i=0; i<(LCD_W*LCD_H); i+=LCD_W){
      memcpy( lcd_buffer+i, lcd_buffer+i+LCD_W, LCD_W );
    }
    memset( lcd_buffer + LCD_W*(LCD_H-1), ' ', LCD_W );
    lcd_cur_x = 0;
    lcd_cur_y = LCD_H-1;
  }
  buf_update_flag = 1;
}
static void newline(char ch);

void lcd_putc_raw(uint32_t ch){
  //newline(ch);
  //dynamic table must override static table
  for(int i=0; i<(sizeof(usersym)/sizeof(usersym[0])); i++){
    if(ch == usersym[i].sym){
      ch = i;
      goto CHAR_FOUND;
    }
  }
  for(int i=0; i<(sizeof(recode_dynamic)/sizeof(recode_dynamic[0])); i++){
    if(ch == recode_dynamic[i].sym){
      ch = recode_dynamic[i].code;
      goto CHAR_FOUND;
    }
  }
  
  if( (ch>=0x20) && (ch<=0x7A) ){ // ASCII
    //do nothing
  }else if( (ch>=L'А') && (ch<=L'ё') ){
    ch = recode_cyr[ch - L'А'];
  }else{
    for(int i=0; i<(sizeof(recode_static)/sizeof(recode_static[0])); i++){
      if(ch == recode_static[i].sym){
        ch = recode_static[i].code;
        goto CHAR_FOUND;
      }
    }
    ch = 0xF8;
  }
  CHAR_FOUND:;
  
  newline(' ');
  
  if(lcd_cur_y >= LCD_H){
    do_newline();
  }
  if( lcd_cur_x < LCD_W ){
    lcd_buffer[ lcd_cur_x + lcd_cur_y*LCD_W ] = ch;
    lcd_cur_x++;
    buf_update_flag = 1;
  }// else - out of screen, ignore
}

static void newline(char ch){
  static char prev = 0;
  if(newline_mode == NL_IGNORE)return;
  if(lcd_cur != CUR_NONE){ //cursor is visible; apply NL immediatly
    if(newline_mode == NL_NORMAL){
      if(ch=='\r'){lcd_cur_x = 0; buf_update_flag = 1;}else if(ch=='\n')do_newline();
    }else{
      if( (ch=='\r')||(ch=='\n') ){
        if( ((prev=='\r')||(prev=='\n')) && (prev!=ch) )return;
        do_newline();
      }
    }
  }else{ //cursor is hidden
    if( newline_mode == NL_NORMAL ){
      if( ch == '\r' )lcd_cur_x = 0;
      else if(ch == '\n'){
        lcd_cur_y++; lcd_cur_x = 0;
        if(lcd_cur_y > LCD_H){do_newline(); lcd_cur_y=LCD_H;}
      }
    }else{
      if( (ch=='\r')||(ch=='\n') ){
        if( ((prev=='\r')||(prev=='\n')) && (prev!=ch) )return;
        lcd_cur_y++; lcd_cur_x = 0;
        if(lcd_cur_y > LCD_H){do_newline(); lcd_cur_y=LCD_H;}
      }
    }
  }
  prev = ch;
}

static void decode_esc_H(char *str){
  uint32_t x=0, y=0;
  str += 2;
  while( (str[0]>='0') && (str[0]<='9') ){
    y = y*10 + str[0] - '0';
    str++;
  }
  if( str[0] == ';' ){
    str++;
    while( (str[0]>='0') && (str[0]<='9') ){
      x = x*10 + str[0] - '0';
      str++;
    }
  }else x = 1;
  
  lcd_cur_x = x-1;
  if(lcd_cur_x >= LCD_W)lcd_cur_x = LCD_W-1;
  lcd_cur_y = y-1;
  if(lcd_cur_y >= LCD_H)lcd_cur_y = LCD_H-1;
  buf_update_flag = 1;
}

static void decode_cur_type(char *str){
  if( (str[3]==' ')&&(str[4]=='q') ){
    switch(str[2]){
      case '1': lcd_cur = CUR_LARGE; break;
      case '4': lcd_cur = CUR_SMALL; break;
      case '7': lcd_cur = CUR_BOTH; break;
    }
  }else if((str[2]=='?')&&(str[3]=='2')&&(str[4]=='5')&&(str[5]=='l')){
    lcd_cur = CUR_NONE;
  }
}

static void decode_special(char *str){
  switch(str[2]){
    case '0': newline_mode = NL_NORMAL; break;
    case '1': newline_mode = NL_NEWLINE; break;
    case '2': newline_mode = NL_IGNORE; break;
  }
}



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

void lcd_putc(char ch){
  static uint32_t wc = 0;
  static char esc[20];
  static int cnt = 0;
  if(ch <= 0b01111111){ // 0xxxxxxx, 1-byte char
    if(cnt == 0){
      if(ch < 0x20){
        switch(ch){
          case '\b': if(lcd_beep_func != NULL)lcd_beep_func(); break;
          case '\e': esc[0] = '\e'; cnt = 1; break;
          case '\n': case '\r':
            newline(ch);
            break;
          case '\t': lcd_cur_x = (lcd_cur_x + 8) &~ 7; buf_update_flag = 1; break;
        }
      }else{
        lcd_putc_raw(ch);
      }
    }else{
      if((ch == '[')||((ch>='0')&&(ch<='9'))||(ch==';')||(ch=='?')||(ch==' ')){
        esc[cnt] = ch;
        cnt++;
      }else{
        esc[cnt] = ch;
        esc[cnt+1]=0;
        if(ch == 'H'){
          decode_esc_H(esc);
        }else if(ch == 'J'){
          memset(lcd_buffer, ' ', sizeof(lcd_buffer));
          buf_update_flag = 1;
          lcd_cur_x = lcd_cur_y = 0;
        }else if( (ch == 'q')||(ch == 'l')){
          decode_cur_type(esc);
        }else if(ch == '.'){
          decode_special(esc);
        }
        cnt = 0;
      }
    }
  }else if(ch <= 0b10111111){ // 10xxxxxx, tail of multi-byte char
    wc = (wc<<6) | (ch & 0b00111111);
    if(wc <= 0x37FF){ // 110xxxxx xxxxxx, 2-byte char
      wc &= 0x7FF;
      lcd_putc_raw(wc);
      wc = 0;
    }else if( wc < 0xE0000 ){ // 1110xxxx xxxxxx (expected next: 10xxxxxx)
      //do nothing: part of 3 or 4-byte char
    }else if( wc <= 0xEFFFF ){ // 1110xxxx xxxxxx xxxxxx, 3-byte char
      wc &= 0xFFFF;
      lcd_putc_raw(wc);
      wc = 0;
    }else if( wc < 0x3C00000 ){ // 11110xxx xxxxxx xxxxxx (expected next: 10xxxxxx)
      // do nothing: part of 4-byte char
    }else if( wc <= 0x3DFFFFF ){ // 11110xxx xxxxxx xxxxxx xxxxxx, 4-byte char
      wc &= 0x3FFFFF;
      lcd_putc_raw(wc);
      wc = 0;
    }else{
      //printf("Error\n");
      wc = 0;
    }
  }else{ //110xxxxx, 1110xxxx, 11110xxx - head of multi-byte char
    wc = ch;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////  VirFat   /////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
#ifndef STRMATCH_FUNC
  #error Define STRMATCH_FUNC
#endif

//timer_chval(LCD_BL_TIM) = (uint32_t)val * marg4(LCD_BL_TIM) / 100;
//lcd_cont_val = (100 - cont) * 4096 / 100;
#define LCDCFG_STR1          "Brightness (0 - 100): "
#define LCDCFG_STR2 "100  \r\nContrast (0 - 100): "
#define LCDCFG_STR3 "100  \r\nCursor (0 - hide, 1 - bar, 2 - underline, 3 - bar+underline): "
#define LCDCFG_STR4 "0    \r\nCR, LF mode (0 - normal, 1 - CR,LF are newline, 2 - ignore): "
#define LCDCFG_STR5 "1    \r\n"
#define LCDCFG_STR_BR	LCDCFG_STR1
#define LCDCFG_STR_CONT	LCDCFG_STR_BR LCDCFG_STR2
#define LCDCFG_STR_CUR	LCDCFG_STR_CONT LCDCFG_STR3
#define LCDCFG_STR_NL	LCDCFG_STR_CUR LCDCFG_STR4
#define LCDCFG_STR		LCDCFG_STR_NL LCDCFG_STR5
void vf_lcdcfg_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  const char cfg_template[] = LCDCFG_STR;
  memcpy(buf, (char*)cfg_template, sizeof(cfg_template));
  memset(buf + sizeof(cfg_template), ' ', 512-sizeof(cfg_template));
  
  int32_t val = lcd_bl_val; //timer_chval(LCD_BL_TIM) * 100 / marg4(LCD_BL_TIM);
  fpi32tos_inplace((char*)(buf+sizeof(LCDCFG_STR_BR)-1), val, 0, 3);
  
  val = 100 - 100 * lcd_cont_val / 4096;
  fpi32tos_inplace((char*)(buf+sizeof(LCDCFG_STR_CONT)-1), val, 0, 3);
  
  ((char*)buf)[ sizeof(LCDCFG_STR_CUR)-1 ] = lcd_cur + '0';
  ((char*)buf)[ sizeof(LCDCFG_STR_NL)-1 ] = newline_mode + '0';
}

void vf_lcdcfg_write(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  uint32_t val = 0;
  buf[511] = 0;
  char *en = (char*)buf + 512;
  
  char *ch = strstr((char*)buf, "Brightness (0 - 100):");
  if(ch != NULL){
    ch += sizeof("Brightness (0 - 100):");
    while( (ch[0]==' ')||(ch[0]=='\t') ){if(ch[0]==0)return; ch++; }
    while( (ch[0]>='0')&&(ch[0]<='9') ){
      val = val*10 + ch[0] - '0';
      ch++;
    }
    if(val > 100)val = 100;
    lcd_bl(val);
  }
  
  ch = strstr((char*)buf, "Contrast (0 - 100):");
  if(ch != NULL){
    val = 0;
    ch += sizeof("Contrast (0 - 100):");
    while( (ch[0]==' ')||(ch[0]=='\t') ){if(ch[0]==0)return; ch++; }
    while( (ch[0]>='0')&&(ch[0]<='9') ){
      val = val*10 + ch[0] - '0';
      ch++;
    }
    if(val > 100)val = 100;
    lcd_cont(val);
  }
  
  ch = strstr((char*)buf, "Cursor");
  while(ch != NULL){
    for(;ch<en; ch++){
      if(ch[0] == ':'){ch++; break;}
      if( (ch[0]=='\r')||(ch[0]=='\n') ){ch = NULL; break;}
    }
    if((ch == NULL)||(ch >= en))break;
    for(;ch<en; ch++){
      if((ch[0]>='0')&&(ch[0]<='3'))break;
      if((ch[0]!=' ')&&(ch[0]!='\t')){ch=NULL; break;}
    }
    if((ch == NULL)||(ch >= en))break;
    lcd_cur = ch[0] - '0';
    break;
  }
  
  ch = strstr((char*)buf, "CR, LF mode");
  while(ch != NULL){
    for(;ch<en; ch++){
      if(ch[0] == ':'){ch++; break;}
      if( (ch[0]=='\r')||(ch[0]=='\n') ){ch = NULL; break;}
    }
    if((ch == NULL)||(ch >= en))break;
    for(;ch<en; ch++){
      if((ch[0]>='0')&&(ch[0]<='2'))break;
      if((ch[0]!=' ')&&(ch[0]!='\t')){ch=NULL; break;}
    }
    if((ch == NULL)||(ch >= en))break;
    newline_mode = ch[0] - '0';
    break;
  }
  
  if(save_flag==0)save_flag = 1;
}

char* strtohex(char *src, uint32_t *dst){
  uint32_t res = 0;
  while(1){
    if( (src[0]>='0')&&(src[0]<='9') ){
      res = (res<<4) | (src[0]-'0');
    }else if( (src[0]>='a')&&(src[0]<='f') ){
      res = (res<<4) | (src[0]-'a' + 0xA);
    }else if( (src[0]>='A')&&(src[0]<='F') ){
      res = (res<<4) | (src[0]-'A' + 0xA);
    }else break;
    src++;
  }
  *dst = res;
  return src;
}

#define utf32_len(x) (((x)<(1<<7))?1: ((x)<(1LU<<11))?2: ((x)<(1LU<<16))?3: 4)

char* utf32toutf8(char *buf, uint32_t sym){
  if( sym < (1<<7)){ //1-byte
    buf[0] = sym;
    return buf+1;
  }else if( sym <= (1LU<<11) ){ //2-byte
    buf[0] = ((sym>> 6) & 0b00011111) | 0b11000000;
    buf[1] = ((sym>> 0) & 0b00111111) | 0b10000000;
    return buf+2;
  }else if( sym < (1LU<<16) ){ //3-byte
    buf[0] = ((sym>>12) & 0b00001111) | 0b11100000;
    buf[1] = ((sym>> 6) & 0b00111111) | 0b10000000;
    buf[2] = ((sym>> 0) & 0b00111111) | 0b10000000;
    return buf+3;
  }else{ //0x10FFFF //4-byte
    buf[0] = ((sym>>18) & 0b00000111) | 0b11110000;
    buf[1] = ((sym>>12) & 0b00111111) | 0b10000000;
    buf[2] = ((sym>> 6) & 0b00111111) | 0b10000000;
    buf[3] = ((sym>> 0) & 0b00111111) | 0b10000000;
    return buf+4;
  }
}

uint32_t utf8toutf32(char *buf){
  if(buf[0] < 0b10000000){
    return buf[0];
  }else if(buf[0] < 0b11100000){
    return ((((uint32_t)buf[0])&0b00011111)<<6) | (buf[1] & 0b00111111);
  }else if(buf[0] < 0b11110000){
    return ((((uint32_t)buf[0])&0b00001111)<<12) | ((((uint32_t)buf[1])&0b01111111)<<6) | (buf[2] & 0b00111111);
  }else{
    return ((((uint32_t)buf[0])&0b00000111)<<18) | ((((uint32_t)buf[1])&0b00111111)<<12) | ((((uint32_t)buf[2])&0b01111111)<<6) | (buf[3] & 0b00111111);
  }
}

void vf_usersym_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  const char template[] = 
    "  Symbol: [1]    \r\n"
    "# HEX: 0x12345678 \r\n"
    "Pixels:\r\n";
  usersym_t *us = &(usersym[ (uint32_t)(virfat_rootdir[file_idx].userdata) ]);
  
  memcpy(buf, (char*)template, sizeof(template));
  
  char *ch = utf32toutf8( (char*)buf+sizeof("  Symbol: [")-1, us->sym);
  ch[0] = ']';
  
  ch = u32tohex( (char*)buf + sizeof("  Symbol: [1]    \r\n# HEX: 0x")-1, us->sym, 8 );
  ch[8] = ' ';
  
  ch = (char*)buf + sizeof(template);
  for(int i=0; i<8; i++){
    ch[-1] = '|';
    uint8_t mask = (1<<4);
    for(int j=0; j<5; j++){
      if(us->data[i] & mask)ch[0] = '#'; else ch[0] = ' ';
      mask >>= 1;
      ch++;
    }
    ch[0]='|'; ch[1]='\r'; ch[2]='\n'; ch+=4;
  }
  for(ch-=1; ch < ((char*)buf + 512); ch++)ch[0] = ' ';
}

void vf_usersym_write(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  usersym_t *us = &(usersym[ (uint32_t)(virfat_rootdir[file_idx].userdata) ]);
  buf[511] = 0;
  uint8_t data[8] = {0,0,0,0,0,0,0,0};
  uint32_t sym = 0;
  
  //decode Symbol
  char *ch1 = strstr((char*)buf, "Symbol");
  while(ch1 != NULL){
    for(char *c = ch1-1; c >= (char*)buf; c--){
      if((c[0] == '\r') || (c[0] == '\n'))break;
      if((c[0] == ' ') || (c[0] == '\t'))continue;
      ch1 = NULL;
    }
    if(ch1 == NULL)break;
    
    ch1 += sizeof("Symbol")-1;
    for(; ch1[0]!='['; ch1++){
      if((ch1[0]=='\r')||(ch1[0]=='\n')||(ch1[0]==0)){ch1 = NULL; break;}
    }
    if(ch1 == NULL)break;
    sym = utf8toutf32(ch1+1);
    break;
  }
  
  //decode HEX
  char *ch2 = strstr((char*)buf, "HEX");
  while(ch2 != NULL){
    for(char *c = ch2-1; c >= (char*)buf; c--){
      if((c[0] == '\r') || (c[0] == '\n'))break;
      if((c[0] == ' ') || (c[0] == '\t'))continue;
      ch2 = NULL;
    }
    if(ch2 == NULL)break;
    ch2 += sizeof("HEX")-1;
    for(; ch2[0]!='0'; ch2++){
      if((ch2[0]=='\r')||(ch2[0]=='\n')||(ch2[0]==0)){ch2 = NULL; break;}
    }
    if(ch2 == NULL)break;
    if(ch2[1] != 'x')break;
    ch2+=2;
    sym = 0;
    while(1){
      if( (ch2[0]>='0')&&(ch2[0]<='9') ){
        sym = (sym<<4) + (ch2[0] - '0');
      }else if( (ch2[0]>='a')&&(ch2[0]<='f') ){
        sym = (sym<<4) + (ch2[0] - 'a' + 0xA);
      }else if( (ch2[0]>='A')&&(ch2[0]<='F') ){
        sym = (sym<<4) + (ch2[0] - 'A' + 0xA);
      }else{
        break;
      }
      ch2++;
    }
    break;
  }
  
  //decode data
  if(ch2 > ch1)ch1 = ch2;
  if(ch1 == NULL)return; // Neither 'Symbol' nor 'HEX' defined; Error
  ch1 = strstr(ch1, "Pixels:");
  if(ch1 == NULL)return;
  ch1 += sizeof("Pixels:")-1;
  //UART_puts(UART_DECLARATIONS, ch1);
  for(int i=0; i<8; i++){
    ch1 = strstr(ch1, "|");
    //UART_puts(UART_DECLARATIONS, ch1);
    if(ch1 == NULL)return;
    if(ch1[6] != '|')return;
    uint8_t mask = (1<<4);
    for(int j=0; j<5; j++){
      ch1++;
      if((ch1[0]!=' ')&&(ch1[0]!='0'))data[i] |= mask;
      mask >>= 1;
    }
    ch1+=2;
  }
  us->sym = sym;
  memcpy(us->data, data, sizeof(data));
  
  usersym_update = 1;
  if(save_flag==0)save_flag = 1;
}

void vf_usertbl_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  recode_t *tbl = &(recode_dynamic[ 16*(uint32_t)(virfat_rootdir[file_idx].userdata) ]);
  char *ch = (char*)buf;
  for(int i=0; i<16; i++){
    if(tbl[i].disp_as_hex){
      ch[0] = '[';
      char *res = utf32toutf8(ch+1, tbl[i].sym);
      ch += 7;
      for(;res<ch; res++)res[0]=' ';
      ch[0] = ']';
      ch[1] = '=';
      u32tohex(ch+2, tbl[i].code, 2);
      ch[4] = '\r';
      ch[5] = '\n';
      ch += 6;
    }else{
      u32tohex(ch, tbl[i].sym, 8);
      ch[8] = '=';
      u32tohex(ch+9, tbl[i].code, 2);
      ch[11] = '\r';
      ch[12] = '\n';
      ch += 13;
    }
  }
  for(;ch < (char*)buf + 512; ch++)ch[0] = ' ';
}

void vf_usertbl_write(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  recode_t *tbl = &(recode_dynamic[ 16*(uint32_t)(virfat_rootdir[file_idx].userdata) ]);
  recode_t tmp[16];
  char *ch = (char*)buf;
  char *en = ch + 512;
  uint32_t idx = 0;
  buf[511] = 0;
  while((ch < en)&&(idx<16)){
    //read symbol
    uint32_t sym = 0;
    if(ch[0] == '['){
      sym = utf8toutf32(ch + 1);
      ch += 2;
      for(;ch[0] != ']'; ch++){
        if((ch[0] == '\r')||(ch[0]=='\n')||(ch>=en))return;
      }
      ch++;
      tmp[idx].disp_as_hex = 0;
    }else{
      char *res = strtohex(ch, &sym);
      if(res == ch)return;
      ch = res;
      tmp[idx].disp_as_hex = 0;
    }
    
    //read code
    if(ch[0] != '=')return;
    uint32_t code;
    char *res = strtohex(ch+1, &code);
    if(res == (ch+1))return;
    if(code > 0xFF)return;
    tmp[idx].sym = sym;
    tmp[idx].code = code;
    idx++;
    ch = res;
    
    //next line
    for(;((ch[0]==' ')||(ch[0]=='\r')||(ch[0]=='\n')||(ch[0]=='\t'))&&(ch<en); ch++){}
  }
  
  if(idx != 16)return;
  memcpy(tbl, tmp, sizeof(tmp));
  if(save_flag==0)save_flag = 1;
}

const char lcd_help1[] = LCD_HELP1;
const char lcd_help3[] = LCD_HELP3;
#define H1ST 0
#define H1EN (H1ST + sizeof(lcd_help1))
#define H2ST (H1EN)
#define H2EN (H2ST + sizeof(LCD_HELP2))
#define H3ST (H2EN)
#define H3EN (H3ST + sizeof(lcd_help3))
void vf_lcdhelp_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  addr *= 512;
  uint32_t eaddr = addr + 512;
  uint32_t st, en=addr;
  
  if((H1ST <= eaddr) && (H1EN >= addr) ){
    st = addr;
    if(H1ST > st)st = H1ST;
    en = eaddr;
    if(H1EN < en)en = H1EN;
    memcpy( (char*)buf + (st-addr), (char*)lcd_help1 + (st-H1ST), (en-st) );
  }
  
  if((H2ST <= eaddr) && (H2EN >= addr) ){
    char lcd_help2[sizeof(LCD_HELP2)];
    char *ch = lcd_help2;
    for(int i=0; i<(sizeof(usersym)/sizeof(usersym[0])); i++){
      ch = utf32toutf8(ch, usersym[i].sym);
    }
    for(;ch<(lcd_help2+sizeof(lcd_help2)-2); ch++)ch[0] = ' ';
    ch[0] = '\r'; ch[1] = '\n';
    
    st = addr;
    if(H2ST > st)st = H2ST;
    en = eaddr;
    if(H2EN < en)en = H2EN;
    memcpy( (char*)buf + (st-addr), lcd_help2 + (st-H2ST), (en-st) );
  }
  
  if((H3ST <= eaddr) && (H3EN >= addr) ){
    st = addr;
    if(H3ST > st)st = H3ST;
    en = eaddr;
    if(H3EN < en)en = H3EN;
    memcpy( (char*)buf + (st-addr), (char*)lcd_help3 + (st-H3ST), (en-st) );
  }
  
  if(en < eaddr)memset( (char*)buf + (en-addr), ' ', (eaddr-en) );
}