#ifndef __BEEP_H__
#define __BEEP_H__

//TSND2
//#define SND		A,9,1,GPIO_APP50
//#define TSND	1,2,TIMO_PWM_NINV | TIMO_POS

/*
static uint32_t snd_freq_Hz = 500; //TODO: FLASH
static uint32_t snd_dur_ms = 1000; //TODO: FLASH
static uint32_t snd_vol = 10; // 0 - 100 //TODO: FLASH
*/
#include "savedata.h"
#define snd_freq_Hz (device_settings.snd.freq_Hz)
#define snd_dur_ms	(device_settings.snd.dur_ms)
#define snd_vol		(device_settings.snd.vol)
static uint32_t snd_t_off = 0;
static uint8_t snd_en = 0;

const uint8_t sin256[256] = {
  128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173, 176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220, 218, 215, 213, 211, 208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179, 176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 134, 131, 128, 124, 121, 118, 115, 112, 109, 106, 103, 100, 97, 93, 90, 88, 85, 82, 79, 76, 73, 70, 67, 65, 62, 59, 57, 54, 52, 49, 47, 44, 42, 40, 37, 35, 33, 31, 29, 27, 25, 23, 21, 20, 18, 17, 15, 14, 12, 11, 10, 9, 7, 6, 5, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 5, 6, 7, 9, 10, 11, 12, 14, 15, 17, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35, 37, 40, 42, 44, 47, 49, 52, 54, 57, 59, 62, 65, 67, 70, 73, 76, 79, 82, 85, 88, 90, 93, 97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 
};

uint8_t snd_scale[ sizeof(sin256) ];

static void snd_recalc(){
  // ARR = (F_CPU / 256 / freq_Hz)
  if(snd_vol>100)snd_vol = 100;
  for(int i=0; i<sizeof(sin256); i++){
    snd_scale[i] = (uint32_t)sin256[i] * snd_vol / 100;
  }
  TIMx(TSND2)->ATRLR = 144000000 / 256 / snd_freq_Hz;
}

void snd_init(){
  GPIO_config(SND);
  timer_chval(TSND)=10;
  timer_chcfg(TSND);
  TIMO_DEF(TSND);
  
  timer_init(TSND2, 0, 5625);
  timer_chval(TSND2)=1;
  
  snd_recalc();
  
  dma_clock(timer_dma(TSND2), 1);
  dma_cfg_io(timer_dma(TSND2), &timer_chval(TSND), snd_scale, sizeof(sin256));
  dma_cfg_mem(timer_dma(TSND2), 16, 0, 8, 1, 1, DMA_PRI_LOW);
  dma_disable( timer_dma(TSND2) );
  
  timer_enable(TSND2);
}

void snd_start(){
  dma_enable( timer_dma(TSND2) );
  GPIO_manual(SND, GPIO_APP50);
  snd_t_off = systick_read32() + 144000000 / 1000 * snd_dur_ms;
  snd_en = 1;
}

static void snd_poll(uint32_t time){
  if(!snd_en)return;
  if( (int32_t)(time - snd_t_off) < 0)return;
  GPIO_manual(SND, GPIO_PP50);
  GPO_OFF(SND);
  dma_disable( timer_dma(TSND2) );
  snd_en = 0;
}


////// VirFat ///////////////////
#define SND_STR_1            "Frequency, Hz: "
#define SND_STR_2 "20000  \r\nVolume (0 - 100): "
#define SND_STR_3   "100  \r\nDuration, ms: "
#define SND_STR_4 "100000  \r\n"
#define SND_STR_FREQ		SND_STR_1
#define SND_STR_VOL			SND_STR_FREQ SND_STR_2
#define SND_STR_DUR			SND_STR_VOL SND_STR_3
#define SND_STR				SND_STR_DUR SND_STR_4
void vf_snd_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  const char snd_str[] = SND_STR;
  memcpy(buf, (void*)snd_str, sizeof(snd_str));
  memset((char*)buf + sizeof(snd_str)-1, ' ', 512 - sizeof(snd_str));
  
  fpi32tos_inplace( (char*)buf + sizeof(SND_STR_FREQ)-1, snd_freq_Hz, 0, 5);
  fpi32tos_inplace( (char*)buf + sizeof(SND_STR_VOL)-1, snd_vol, 0, 3);
  fpi32tos_inplace( (char*)buf + sizeof(SND_STR_DUR)-1, snd_dur_ms, 0, 6);
}

void vf_snd_write(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  char *ch;
  char *en = (char*)buf + 512;
  buf[511] = 0;
  
  ch = strstr((char*)buf, "Frequency");
  while(ch != NULL){
    ch += sizeof("Frequency");
    for(;ch[0]!=':'; ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    ch++;
    for(; (ch[0]<'0')||(ch[0]>'9'); ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    uint32_t val = 0;
    for(; (ch[0]>='0')&&(ch[0]<='9'); ch++){
      val = val*10 + ch[0] - '0';
    }
    snd_freq_Hz = val;
    break;
  }
  
  ch = strstr((char*)buf, "Volume");
  while(ch != NULL){
    ch += sizeof("Volume");
    for(;ch[0]!=':'; ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    ch++;
    for(; (ch[0]<'0')||(ch[0]>'9'); ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    uint32_t val = 0;
    for(; (ch[0]>='0')&&(ch[0]<='9'); ch++){
      val = val*10 + ch[0] - '0';
    }
    if(val > 100)val = 100;
    snd_vol = val;
    break;
  }
  
  ch = strstr((char*)buf, "Duration");
  while(ch != NULL){
    ch += sizeof("Duration");
    for(;ch[0]!=':'; ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    ch++;
    for(; (ch[0]<'0')||(ch[0]>'9'); ch++){
      if( (ch[0]=='\r')||(ch[0]=='\n')||(ch>=en) ){ch=NULL; break;}
    }
    if(ch == NULL)break;
    uint32_t val = 0;
    for(; (ch[0]>='0')&&(ch[0]<='9'); ch++){
      val = val*10 + ch[0] - '0';
    }
    snd_dur_ms = val;
    break;
  }
  
  snd_recalc();
  snd_start();
}

#endif