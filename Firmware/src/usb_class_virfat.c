#include "usbd_lib.h"
#include "hardware.h"
#define MSD_VIRFAT_IMPL
#include "usb_class_virfat.h"

#define USBCLASS_MSC_GET_MAX_LUN	0xFE
#define USBCLASS_MSC_RESET			0xFF
#define MSC_MEDIA_PACKET			512

//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//------ VirFat code       ----------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//#define VIRFAT_READONLY
#ifndef VIRFAT_DATE_DD_MM_YYYY
  #define VIRFAT_DATE_DD_MM_YYYY	__DATE_D__, __DATE_M__, __DATE_Y__
#endif
#ifndef VIRFAT_TIME_HH_MM_SS
  #define VIRFAT_TIME_HH_MM_SS (__TIME_S__/3600), (__TIME_S__/60%60), (__TIME_S__%60)
#endif
#ifndef VIRFAT_VOLID
  #define VIRFAT_VOLID		0xFC561629
#endif
#ifndef VIRFAT_JMPBOOT
  #define VIRFAT_JMPBOOT		{0xEB, 0x3C, 0x90}
#endif
#ifndef VIRFAT_OEMNAME
  #define VIRFAT_OEMNAME		"virfat  "
#endif
#define VIRFAT_FSNAME		"FAT16   "

void virfat_file_dummy(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  for(uint16_t i=0; i<512; i++)buf[i] = ' ';
}

#define VIRFAT_FILES_TOTAL	( sizeof(virfat_rootdir) / sizeof(virfat_file_t) )
#ifdef VIRFAT_VOLNAME
  #define VIRFAT_ROOTENT (( VIRFAT_FILES_TOTAL + 15 + 1 ) &~ 15)
#else
  #define VIRFAT_ROOTENT (( VIRFAT_FILES_TOTAL + 15) &~ 15)
#endif

#define virfat_pbrstart 0
#define virfat_fatstart 1
static uint16_t virfat_rootstart;
#define virfat_datastart (virfat_rootstart + (VIRFAT_ROOTENT / 16))

#define _FAT_DATE(day, month, year) ( ((((year)-1980) & 127)<<9) | (((month)&15)<<5) | ((day) & 31) )
#define FAT_DATE(x) _FAT_DATE(x)
#define _FAT_TIME(hh, mm, ss) ( (((hh) & 31)<<11) | (((mm) & 63)<<5) | (((ss)/2) & 31) )
#define FAT_TIME(x) _FAT_TIME(x)
typedef struct{
  uint16_t day:5;
  uint16_t month:4;
  uint16_t year_1980:7;
}virfat_date_t;
typedef struct{
  uint16_t sec2:5;
  uint16_t min:6;
  uint16_t hour:5;
}virfat_time_t;

#pragma pack(push, 1)
typedef struct __attribute__((__packed__)){
  uint8_t JmpBoot[3]; //ignore
  uint8_t OEMname[8];
  uint16_t BytesPerSec; //512
  uint8_t SecPerClust; //1
  uint16_t SecReserved; //1
  uint8_t NumFats; //1
  uint16_t RootEntCnt; //32 (кратно 32)
  uint16_t TotSect; //0x2000 (от 0x0FFF - это fat12)
  uint8_t DriveType; //F8 - HDD
  uint16_t fatsize; //0x0020 (TotSect / BytesPerSec / SecPerClust * 2)
  uint16_t SecPerTrak; //0x0020 ?
  uint16_t NumHeads; //0x0040
  uint32_t SecHidden; //0
  uint32_t TotSect32; //0
  
  uint8_t DriveNum; //0x80 ?
  uint8_t NTErrFlag; //0
  uint8_t BootSig; //0x29 ?
  uint32_t VolID; //0xFC561629?
  char VolName[11]; //строка
  char FSName[8]; //"FAT16   "
  //reserved[]
  //[0x01FE] = 0x55
  //[0x01FF] = 0xAA
}virfat_pbr_t;
static const virfat_pbr_t virfat_pbr = {
  .JmpBoot = VIRFAT_JMPBOOT,
  .OEMname = VIRFAT_OEMNAME,
  .BytesPerSec = 512,
  .SecPerClust = 1,
  .SecReserved = 1,
  .NumFats = 1,
  .RootEntCnt = VIRFAT_ROOTENT,
  //.TotSect = VIRFAT_TOTSECT,
  .DriveType = 0xF8,
  //.fatsize = VIRFAT_FAT_SIZE, //(TotSect / BytesPerSec / SecPerClust * 2)
  .SecPerTrak = 0x0020, //?
  .NumHeads = 0x0040, //?
  .SecHidden = 0,
  .TotSect32 = 0,
  .DriveNum = 0x80, //?
  .NTErrFlag = 0, //?
  .BootSig = 0x29,//0, //?
  .VolID = VIRFAT_VOLID,
#ifdef VIRFAT_VOLNAME
  .VolName = VIRFAT_VOLNAME,
#else
  .VolName = "___________",
#endif
  .FSName = VIRFAT_FSNAME,
};
#pragma pack(pop)

static uint16_t virfat_cur_date = FAT_DATE( VIRFAT_DATE_DD_MM_YYYY );
static uint16_t virfat_cur_time = FAT_TIME( VIRFAT_TIME_HH_MM_SS );

#define CLUST_BROKEN 0xFFF7

#pragma pack(push, 1)
typedef struct __attribute__((__packed__)) {
  char name[11];
  uint8_t dir_attr;
  uint8_t NTattr; //ignore
  uint8_t create_time_10ms;
  uint16_t create_time_s;
  uint16_t creae_date;
  uint16_t acc_date;
  uint16_t cluster1_HI;
  uint16_t write_time;
  uint16_t write_date;
  uint16_t cluster1_LO;
  uint32_t size;
}dir_elem;
#pragma pack(pop)
#define VIRFAT_FLAG_RO		0x01
#define VIRFAT_FLAG_HIDDEN	0x02
#define VIRFAT_FLAG_SYSTEM	0x04
#define VIRFAT_FLAG_VOLID	0x08
#define VIRFAT_FLAG_DIR		0x10
#define VIRFAT_FLAG_ARCHIVE	0x20

uint32_t virfat_getsize();
/////////////////////////////////////////////////////
/////// Read  ///////////////////////////////////////
/////////////////////////////////////////////////////
static inline void _virfat_read_pbr(uint8_t *buf, uint32_t addr){
  uint16_t i;
  for(i=0; i<sizeof(virfat_pbr); i++){
    buf[i] = ((uint8_t*)&virfat_pbr)[i];
  }
  ((virfat_pbr_t*)buf)->TotSect = virfat_getsize();
  ((virfat_pbr_t*)buf)->fatsize = (virfat_rootstart - virfat_fatstart);
  for(; i< 0x01FE; i++)buf[i] = 0;
  buf[0x01FE] = 0x55;
  buf[0x01FF] = 0xAA;
}
static inline void _virfat_read_fat(uint8_t *buf, uint32_t addr){
  uint16_t *fat = (uint16_t*)buf;
  uint16_t *fat_end = (uint16_t*)(buf + 512);
  uint16_t sec = (addr-1)*(512/2);
  if(addr == 1){
    fat[0] = 0xFFF8;
    fat[1] = 0xFFFF;
    fat[2] = 0x0000;
    fat += 3;
    sec += 3;
  }
  sec -=3; //3 sectors reserved
  sec += 1; //file addressing from (start+1) to (end)
  uint16_t next_file_addr = 0;
  for(uint16_t i=0; i<VIRFAT_FILES_TOTAL; i++){
    next_file_addr += virfat_rootdir[i].size;
    for(;sec < next_file_addr; sec++){
      fat[0] = sec + 3;
      fat++;
      if(fat == fat_end)return;
    }
    if( sec == next_file_addr){
      sec++;
      fat[0] = 0xFFFF;
      fat++;
      if(fat == fat_end)return;
    }
  }
  
  for(;fat < fat_end; fat++){
    fat[0] = CLUST_BROKEN;
  }
}

static inline void _virfat_read_root(uint8_t *buf, uint32_t addr){
  uint16_t file_addr = 3;
  uint16_t i;
  dir_elem *elem = (dir_elem*)buf;
  
  i = (addr - virfat_rootstart) * 16;
  for(uint16_t j=0; j<i; j++){
    file_addr += virfat_rootdir[j].size;
  }
  
  uint16_t cnt = 0;
  
#ifdef VIRFAT_VOLNAME
  if(i == 0){
    for(uint16_t j=0; j<11; j++)elem[0].name[j] = VIRFAT_VOLNAME[j];
    elem[0].dir_attr = VIRFAT_FLAG_VOLID;
    elem[0].acc_date = elem[0].write_date = virfat_cur_date;
    elem[0].write_time = virfat_cur_time;
    elem[0].cluster1_HI = 0;
    elem[0].cluster1_LO = 0;
    elem[0].size = 0;
    cnt++;
    elem++;
    i++;
  }
  i--;
#endif
  
  for(; i<VIRFAT_FILES_TOTAL; i++, cnt++){
    if(cnt == 16)return;
    for(uint16_t j=0; j<sizeof(dir_elem); j++){ ((uint8_t*)elem)[j] = 0; }
    elem[0].dir_attr = VIRFAT_FLAG_ARCHIVE;
    //if write operations does not supports -> mark file as read-only
    if(virfat_rootdir[i].file_write == virfat_file_dummy)elem[0].dir_attr |= VIRFAT_FLAG_RO;
    
    elem[0].creae_date = FAT_DATE( VIRFAT_DATE_DD_MM_YYYY );
    elem[0].acc_date = elem[0].write_date = virfat_cur_date;
    elem[0].write_time = virfat_cur_time;
    elem[0].cluster1_HI = 0;
    elem[0].size = (uint32_t)512 * virfat_rootdir[i].size;
    
    for(uint16_t j=0; j<11; j++)elem[0].name[j] = virfat_rootdir[i].name[j];
    elem[0].cluster1_LO = file_addr;
    file_addr += virfat_rootdir[i].size;
    elem++;
  }
  for(; i<VIRFAT_ROOTENT; i++, cnt++){
    if(cnt == 16)return;
    for(uint16_t j=0; j<sizeof(dir_elem); j++){ ((uint8_t*)elem)[j] = 0; }
    for(uint16_t j=0; j<11; j++)elem[0].name[j] = 0;
    elem[0].cluster1_HI = 0xFFFF;
    elem[0].cluster1_LO = 0xFFFF;
    elem[0].size = 0;
    elem++;
  }
}
static inline void _virfat_read_data(uint8_t *buf, uint32_t addr){
  uint16_t file_addr = 1;
  addr = (addr - virfat_datastart);
  for(uint16_t j=0; j<VIRFAT_FILES_TOTAL; j++){
    if( (addr >= file_addr) && (addr < (file_addr + virfat_rootdir[j].size)) ){
      virfat_rootdir[j].file_read(buf, addr - file_addr, j);
      return;
    }
    file_addr += virfat_rootdir[j].size;
  }
  for(uint16_t i=0; i<512; i++){
    *(buf++) = 0xFF;
  }
}

/////////////////////////////////////////////////////
/////// Write  //////////////////////////////////////
/////////////////////////////////////////////////////
#define _virfat_write_pbr(buf,  addr)
#define _virfat_write_fat(buf,  addr)
//static uint16_t virfat_cur_date = FAT_DATE( VIRFAT_DATE_DD_MM_YYYY );
//static uint16_t virfat_cur_time = FAT_TIME( VIRFAT_TIME_HH_MM_SS );
#ifndef VIRFAT_TIME_CALLBACK
  #define  _virfat_write_root(buf, addr)
#else
static inline void _virfat_write_root(uint8_t *buf, uint32_t addr){
  uint16_t date, time, change_flag = 0;
  dir_elem *elem = (dir_elem*)buf;
  for(uint16_t i=0; i<16; i++){
    if(elem[i].name[0] == 0)continue;
    date = elem[0].acc_date;
    if(elem[i].write_date > date)date = elem[i].write_date;
    if(date >= virfat_cur_date){
      if(date > virfat_cur_date)change_flag = 1;
      virfat_cur_date = date;
      time = elem[i].write_time;
      if(time > virfat_cur_time){
        virfat_cur_time = time;
        change_flag = 1;
      }
    }
  }
  if(change_flag)virfat_time_callback(virfat_cur_date, virfat_cur_time);
}
#endif

static inline void _virfat_write_data(uint8_t *buf, uint32_t addr){
  uint16_t file_addr = 1;
  addr = (addr - virfat_datastart);
  for(uint16_t j=0; j<VIRFAT_FILES_TOTAL; j++){
    if( (addr >= file_addr) && (addr < (file_addr + virfat_rootdir[j].size)) ){
      virfat_rootdir[j].file_write(buf, addr - file_addr, j);
      return;
    }
    file_addr += virfat_rootdir[j].size;
  }
}

void virfat_init(){
  uint32_t sectotal = 0;
  for(uint16_t i=0; i<VIRFAT_FILES_TOTAL; i++){
    sectotal += virfat_rootdir[i].size;
  }
  if(sectotal < 0x2000)sectotal = 0x2000;
  virfat_rootstart = virfat_fatstart + ((sectotal * 2 + 511) / 512);
}

uint32_t virfat_getsize(){return 512UL*(virfat_rootstart-virfat_fatstart)/2;}

void virfat_read(uint8_t *buf, uint32_t addr){
  if(addr < virfat_fatstart){
    _virfat_read_pbr( buf, addr);
  }else if(addr < virfat_rootstart){
    _virfat_read_fat( buf, addr);
  }else if(addr < virfat_datastart){
    _virfat_read_root(buf, addr);
  }else{
    _virfat_read_data(buf, addr);
  }
}

void virfat_write(uint8_t *buf, uint32_t addr){
  if(addr < virfat_fatstart){
    _virfat_write_pbr( buf, addr);
  }else if(addr < virfat_rootstart){
    _virfat_write_fat( buf, addr);
  }else if(addr < virfat_datastart){
    _virfat_write_root(buf, addr);
  }else{
    _virfat_write_data(buf, addr);
  }
}





//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//------ MSD code          ----------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------





void scsi_reset();
void scsi_command();
static void msc_ep1_in(uint8_t epnum);
static void msc_ep1_out(uint8_t epnum);
USB_ALIGN volatile static uint16_t maxlun = 0; //15; //больше 2 (maxlun=1) последние винды не умеют

char msd_ep0_in(config_pack_t *req, void **data, uint16_t *size){
  if(req->bRequest == USBCLASS_MSC_RESET){
    scsi_reset();
    return 1;
  }
  if(req->bRequest == USBCLASS_MSC_GET_MAX_LUN){ //MSC_GET_MAX_LUN
    *data = (void*)&maxlun;
    *size = 1;
    return 1;
  }
  return 0;
}
char msd_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size){return 0;}

void msd_init(){
  usb_ep_init(ENDP_MSD_OUT,       USB_ENDP_BULK, ENDP_MSD_SIZE, msc_ep1_out);
  usb_ep_init(ENDP_MSD_IN | 0x80, USB_ENDP_BULK, ENDP_MSD_SIZE, msc_ep1_in);
  scsi_reset();
  virfat_init();
}

void msd_poll(){}

volatile uint32_t cur_sect_addr = 0xFFFFFFFF;
USB_ALIGN volatile uint8_t cur_sect[MSC_MEDIA_PACKET];

#define CSW_STATUS_SUCCESS		0
#define CSW_STATUS_FAILED		1
#define CSW_STATUS_PHASE_ERROR		2
// The sense codes
enum sbc_sense_key {
	SBC_SENSE_KEY_NO_SENSE         = 0x00,
	SBC_SENSE_KEY_RECOVERED_ERROR  = 0x01,
	SBC_SENSE_KEY_NOT_READY        = 0x02,
	SBC_SENSE_KEY_MEDIUM_ERROR	   = 0x03,
	SBC_SENSE_KEY_HARDWARE_ERROR   = 0x04,
	SBC_SENSE_KEY_ILLEGAL_REQUEST  = 0x05,
	SBC_SENSE_KEY_UNIT_ATTENTION   = 0x06,
	SBC_SENSE_KEY_DATA_PROTECT     = 0x07,
	SBC_SENSE_KEY_BLANK_CHECK      = 0x08,
	SBC_SENSE_KEY_VENDOR_SPECIFIC  = 0x09,
	SBC_SENSE_KEY_COPY_ABORTED     = 0x0A,
	SBC_SENSE_KEY_ABORTED_COMMAND  = 0x0B,
	SBC_SENSE_KEY_VOLUME_OVERFLOW  = 0x0D,
	SBC_SENSE_KEY_MISCOMPARE       = 0x0E,
};

enum sbc_asc {
	SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
	SBC_ASC_PERIPHERAL_DEVICE_WRITE_FAULT   = 0x03,
	SBC_ASC_LOGICAL_UNIT_NOT_READY          = 0x04,
	SBC_ASC_UNRECOVERED_READ_ERROR          = 0x11,
	SBC_ASC_INVALID_COMMAND_OPERATION_CODE  = 0x20,
	SBC_ASC_LBA_OUT_OF_RANGE                = 0x21,
	SBC_ASC_INVALID_FIELD_IN_CDB            = 0x24,
	SBC_ASC_WRITE_PROTECTED                 = 0x27,
	SBC_ASC_NOT_READY_TO_READY_CHANGE       = 0x28,
	SBC_ASC_FORMAT_ERROR                    = 0x31,
	SBC_ASC_MEDIUM_NOT_PRESENT              = 0x3A
};

enum sbc_ascq {
	SBC_ASCQ_NA                             = 0x00,
	SBC_ASCQ_FORMAT_COMMAND_FAILED          = 0x01,
	SBC_ASCQ_INITIALIZING_COMMAND_REQUIRED  = 0x02,
	SBC_ASCQ_OPERATION_IN_PROGRESS          = 0x07
};

struct usb_msc_cbw{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
}__attribute__((packed));
typedef struct usb_msc_cbw usb_msc_cbw_t;

struct usb_msc_csw{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
}__attribute__((packed));
typedef struct usb_msc_csw usb_msc_csw_t;

struct usb_msc_sense{
	uint8_t key;
	uint8_t asc;
	uint8_t ascq;
}__attribute__((packed));
typedef struct usb_msc_sense usb_msc_sense_t;

USB_ALIGN volatile usb_msc_cbw_t msc_cbw;
volatile uint8_t msc_cbw_count = 0;
USB_ALIGN volatile usb_msc_csw_t msc_csw = {
  .dSignature = 0x53425355, //волшебное чиселко
};
uint8_t msc_csw_count = 0;
USB_ALIGN usb_msc_sense_t msc_sense = {0,0,0};

static uint32_t bytestowrite = 0;
static uint32_t bytestoread = 0;
static uint32_t bytescount = 0;
USB_ALIGN static uint8_t buffer[MSC_MEDIA_PACKET];

uint32_t start_lba;
uint16_t block_count;
uint16_t cur_count = 0;

static void msc_ep1_in(uint8_t epnum);

static void msc_ep1_out(uint8_t epnum){
  uint32_t sz = 1;
  int left = sizeof(usb_msc_cbw_t) - msc_cbw_count;
  if(left > 0){ //чтение команды
    sz = usb_ep_read(ENDP_MSD_OUT, (uint16_t*)&(((uint8_t*)&msc_cbw)[msc_cbw_count]) );
    msc_cbw_count += sz;
    if(msc_cbw_count == sizeof(usb_msc_cbw_t)){ //команда прочитана полностью
      scsi_command();
    }else return;
  }else if(bytescount < bytestoread){ //если разнести условие, произойдет повторное чтение буфера EP1_OUT, который был прочитан раньше, но size не сброшен (все равно этим железо занимается)
    uint32_t sect = start_lba + bytescount / 512;
    uint16_t offset = bytescount & 511;
    if(sect != cur_sect_addr){
      cur_sect_addr = sect;
    }
    sz = usb_ep_read(ENDP_MSD_OUT, (uint16_t*)&cur_sect[offset]);
    offset += sz;
    cur_count += sz;
    if(offset >= 512){
      virfat_write((uint8_t*)cur_sect, cur_sect_addr);
    }
    bytescount += sz;
  }
  
  if(bytescount < bytestoread)return;
  msc_ep1_in(ENDP_MSD_IN | 0x80);
}

static void msc_ep1_in(uint8_t epnum){
  if(! usb_ep_ready(epnum) )return;
  
  if(bytescount < bytestowrite){
    uint32_t left = bytestowrite - bytescount;
    if(left > ENDP_MSD_SIZE)left = ENDP_MSD_SIZE;
    if(block_count == 0){
      usb_ep_write(ENDP_MSD_IN, (uint16_t*)&buffer[bytescount], left);
    }else{
      //uint8_t lun = msc_cbw.bLUN;
      uint32_t sect = start_lba + bytescount / 512;
      uint16_t offset = bytescount & 511;
      if(sect != cur_sect_addr){
        virfat_read((uint8_t*)cur_sect, sect);
        cur_sect_addr = sect;
      }
      usb_ep_write(ENDP_MSD_IN, (uint16_t*)(&cur_sect[offset]), left);
      //usb_ep_write(ENDP_NUM, &storage[lun].buf[start_lba*512 + bytescount], left);
      cur_count += left;
    }
    bytescount += left;
  }else{
    int32_t left = sizeof(msc_csw) - msc_csw_count;
    if(left > 0){
      if(left > ENDP_MSD_SIZE)left = ENDP_MSD_SIZE;
      usb_ep_write(ENDP_MSD_IN, (uint16_t*)&(((uint8_t*)&msc_csw)[msc_csw_count]), left);
      msc_csw_count += left;
    }else if(left == 0){
      msc_cbw_count = 0;
      msc_csw_count = 0;
      bytestoread = 0;
      bytestowrite = 0;
      bytescount = 0;
      
      block_count = 0;
      cur_count = 0;
    }
  }
}

//SCSI

// Implemented SCSI Commands
#define SCSI_TEST_UNIT_READY	0x00
#define SCSI_REQUEST_SENSE	0x03
#define SCSI_FORMAT_UNIT	0x04
#define SCSI_READ_6		0x08
#define SCSI_WRITE_6		0x0A
#define SCSI_INQUIRY		0x12
#define SCSI_MODE_SENSE_6	0x1A
#define SCSI_SEND_DIAGNOSTIC	0x1D
#define SCSI_READ_CAPACITY	0x25
#define SCSI_READ_10		0x28
#define SCSI_WRITE_10		0x2A

#define SCSI_MMC_START_STOP_UNIT	0x1B
#define SCSI_MMC_PREVENT_ALLOW_REMOVAL	0x1E
#define SCSI_MMC_READ_FORMAT_CAPACITY 0x23 //винда очень любит этот запрос, а коррекно обрабатывать ответ "не поддерживаю" не умеет

typedef struct{
  uint8_t opcode;
  uint8_t cdb_info1;
  uint32_t block_address;
  uint8_t cdb_info2;
  uint16_t length;
  uint8_t control;
}scsi_cbw_10_t;

inline void scsi_reset(){
  msc_cbw_count = 0;
  msc_csw_count = 0;
  bytestoread = 0;
  bytestowrite = 0;
  bytescount = 0;
  
  block_count = 0;
  cur_count = 0;
  //TODO
}

static const uint8_t inquiry_response[36] = {
  0x00,	// Byte 0: Peripheral Qualifier = 0, Peripheral Device Type = 0
  0x80,	// Byte 1: RMB = 1, Reserved = 0
  0x04,	// Byte 2: Version = 0
  0x02,	// Byte 3: Obsolete = 0, NormACA = 0, HiSup = 0, Response Data Format = 2
  0x1F,	// Byte 4: Additional Length (n-4) = 31 + 4
  0x00,	// Byte 5: SCCS = 0, ACC = 0, TPGS = 0, 3PC = 0, Reserved = 0, Protect = 0
  0x00,	// Byte 6: BQue = 0, EncServ = 0, VS = 0, MultiP = 0, MChngr = 0, Obsolete = 0, Addr16 = 0
  0x00,	// Byte 7: Obsolete = 0, Wbus16 = 0, Sync = 0, Linked = 0, CmdQue = 0, VS = 0
  'C', 'O', 'K', 'P', ' ', ' ', ' ', ' ', //Vendor (8 bytes)
  'M', 'S', 'D', ' ', ' ', ' ', ' ', ' ', //Product (16 bytes)
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '0', '.', '0', '1' //Version (4 bytes)
};

#define LENGTH_INQUIRY_PAGE00		 7
const uint8_t  inquiry_page00_data[] = {//7						
  0x00,		
  0x00, 
  0x00, 
  (LENGTH_INQUIRY_PAGE00 - 4),
  0x00, 
  0x80, 
  0x83 
};  

void scsi_inquiry(){
  if( msc_cbw.CB[1] & 0x01){ //EVPD is set. Для Win10 это важно. Для Linux или WinXP - нет
    for(uint16_t i=0; i<sizeof(inquiry_page00_data); i++)buffer[i] = ((uint8_t*)inquiry_page00_data)[i];
    bytestowrite = sizeof(inquiry_page00_data);
  }else{
    for(uint16_t i=0; i<sizeof(inquiry_response); i++)buffer[i] = ((uint8_t*)inquiry_response)[i];
    bytestowrite = sizeof(inquiry_response);
  }
  if( bytestowrite < msc_cbw.dDataLength ) bytestowrite = msc_cbw.dDataLength;
  
  msc_csw.dDataResidue = msc_cbw.dDataLength - bytestowrite;
  msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
  msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
  msc_sense.ascq =SBC_ASCQ_NA;
}

static const uint8_t sense_response[18] = {
  0x70,	// Byte 0: VALID = 0, Response Code = 112
  0x00,	// Byte 1: Obsolete = 0
  0x00,	// Byte 2: Filemark = 0, EOM = 0, ILI = 0, Reserved = 0, Sense Key = 0
  	// Byte 3 - Byte 6: Information = 0
  0, 0, 0, 0,
  0x0a,	// Byte 7: Additional Sense Length = 10
  	// Byte 8 - Byte 11: Command Specific Info = 0
  0, 0, 0, 0,
  0x00,	// Byte 12: Additional Sense Code (ASC) = 0
  0x00,	// Byte 13: Additional Sense Code Qualifier (ASCQ) = 0
  0x00,	// Byte 14: Field Replaceable Unit Code (FRUC) = 0
  0x00,	// Byte 15: SKSV = 0, SenseKeySpecific[0] = 0
  0x00,	// Byte 16: SenseKeySpecific[0] = 0
  0x00	// Byte 17: SenseKeySpecific[0] = 0
};
void scsi_request_sense(){
  //memcpy(buffer, sense_response, 18);
  for(uint8_t i=0; i<sizeof(sense_response); i++)buffer[i] = sense_response[i];
  buffer[2] = msc_sense.key; //0x05
  buffer[12]= msc_sense.asc; //0x20
  buffer[13]= msc_sense.ascq; //0x00
  bytestowrite = 18;
}

void scsi_test_unit_ready(){
  if(virfat_enable){
    //return OK
    msc_csw.dDataResidue = 0;
    msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
    msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
    msc_sense.ascq =SBC_ASCQ_NA;
  }else{
    //return Not ready
    msc_csw.dDataResidue = 0;
    msc_csw.bStatus = CSW_STATUS_FAILED;
    msc_sense.key = SBC_SENSE_KEY_NOT_READY;
    msc_sense.asc = SBC_ASC_MEDIUM_NOT_PRESENT;
    msc_sense.ascq =SBC_ASCQ_NA;
  }
}

void scsi_read_capacity(){
  //uint8_t lun = msc_cbw.bLUN;
  //uint32_t last_lba = storage[lun].capacity / 512 - 1;
  uint32_t last_lba = virfat_getsize() - 1;
  
  buffer[0] = (last_lba >> 24) & 0xFF;
  buffer[1] = (last_lba >> 16) & 0xFF;
  buffer[2] = (last_lba >>  8) & 0xFF;
  buffer[3] = (last_lba >>  0) & 0xFF;
  
  buffer[4] = 0;
  buffer[5] = 0;
  buffer[6] = 2;
  buffer[7] = 0;
  
  bytestowrite = 8;

  msc_csw.dDataResidue = 0;
  msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
  msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
  msc_sense.ascq =SBC_ASCQ_NA;
}

void scsi_mode_sense_6(){
  buffer[0] = 3;
  buffer[1] = 0;
#ifdef VIRFAT_READONLY
  buffer[2] = (1<<7); //read-only?
#else
  buffer[2] = 0;
#endif
  buffer[3] = 0;
  bytestowrite = 4;
  msc_csw.dDataResidue = msc_cbw.dDataLength-4;
}

void scsi_read_10(){
  //uint32_t 
  start_lba = (msc_cbw.CB[2] << 24) | (msc_cbw.CB[3] << 16) | (msc_cbw.CB[4] << 8) | (msc_cbw.CB[5] << 0);
  //uint16_t 
  block_count = (msc_cbw.CB[7] << 8) | (msc_cbw.CB[8] << 0);
  cur_count = 0;
  
  bytestowrite = ((uint32_t)block_count) << 9;
  
  msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
  msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
  msc_sense.ascq =SBC_ASCQ_NA;
}

void scsi_write_10(){
  start_lba = (msc_cbw.CB[2] << 24) | (msc_cbw.CB[3] << 16) | (msc_cbw.CB[4] << 8) | (msc_cbw.CB[5] << 0);
  block_count = (msc_cbw.CB[7] << 8) | (msc_cbw.CB[8] << 0);
  
  //uint8_t lun = msc_cbw.bLUN;
  cur_count = 0;
  
#ifdef VIRFAT_READONLY
    msc_csw.bStatus = CSW_STATUS_FAILED;
    msc_sense.key = SBC_SENSE_KEY_MEDIUM_ERROR;
    msc_sense.asc = SBC_ASC_WRITE_PROTECTED;
    msc_sense.ascq =SBC_ASCQ_NA;
    return;
#endif
  
  bytestoread = ((uint32_t)block_count) << 9;
  
  msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
  msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
  msc_sense.ascq =SBC_ASCQ_NA;
}

void scsi_mmc_read_fmt_cap(){
  //uint8_t lun = msc_cbw.bLUN;
  uint32_t last_lba = virfat_getsize();
  
  buffer[0] = buffer[1] = buffer[2] = 0; //reserved
  buffer[3] = 8;//size
  
  buffer[4] = (last_lba >> 24) & 0xFF;
  buffer[5] = (last_lba >> 16) & 0xFF;
  buffer[6] = (last_lba >>  8) & 0xFF;
  buffer[7] = (last_lba >>  0) & 0xFF;
  
  buffer[8] = 0;
  buffer[9] = 0;
  buffer[10] = 2;
  buffer[11] = 0;
  
  bytestowrite = 12;
  
  msc_sense.key = SBC_SENSE_KEY_NO_SENSE;
  msc_sense.asc = SBC_ASC_NO_ADDITIONAL_SENSE_INFORMATION;
  msc_sense.ascq =SBC_ASCQ_NA;
}

void scsi_command(){
  msc_csw.dTag = msc_cbw.dTag;
  msc_csw.bStatus = CSW_STATUS_SUCCESS;
  msc_csw.dDataResidue = 0;
  
  bytestoread = 0;
  bytestowrite = 0;
  bytescount = 0;
  
  switch( msc_cbw.CB[0] ){
    case SCSI_INQUIRY:
      scsi_inquiry();
      break;
    case SCSI_TEST_UNIT_READY:
      scsi_test_unit_ready();
      break;
    case SCSI_READ_CAPACITY:
      scsi_read_capacity();
      break;
    case SCSI_REQUEST_SENSE:
      scsi_request_sense();
      break;
    case SCSI_MODE_SENSE_6:
      scsi_mode_sense_6();
      break;
    case SCSI_READ_10:
      scsi_read_10();
      break;
    case SCSI_WRITE_10:
      scsi_write_10();
      break;
    case SCSI_MMC_READ_FORMAT_CAPACITY:
      scsi_mmc_read_fmt_cap();
      break;
    case SCSI_MMC_START_STOP_UNIT:
    case SCSI_MMC_PREVENT_ALLOW_REMOVAL:
      break;
    default:
      msc_csw.bStatus = CSW_STATUS_FAILED;
      msc_sense.key = SBC_SENSE_KEY_ILLEGAL_REQUEST;
      msc_sense.asc = SBC_ASC_INVALID_COMMAND_OPERATION_CODE;
      msc_sense.ascq = SBC_ASCQ_NA;
      //
      break;
  }
}

