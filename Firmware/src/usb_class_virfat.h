#ifndef __USB_CLASS_MSD__
#define __USB_CLASS_MSD__

#include <stdint.h>
#include "usbd_lib.h"
#include "hardware.h"
#include "clock.h"
#include "lcd_hd44780.h"
#include "usb_class_hid.h"

char msd_ep0_in(config_pack_t *req, void **data, uint16_t *size);
char msd_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size);
void msd_init();
void msd_poll();

extern volatile uint8_t virfat_enable;

typedef void(*virfat_callback)(uint8_t *buf, uint32_t addr, uint16_t file_idx);

typedef struct{
  char *name;                //file name (in DOS format, upper register, 8.3)
  void *userdata;            //user-specific data
  virfat_callback file_read; //callback Host reading sector from Device
  virfat_callback file_write;//callback Host writing sector to Device
  uint16_t size;             //file size
}virfat_file_t;

extern const virfat_file_t virfat_rootdir[];

void virfat_file_dummy(uint8_t *buf, uint32_t addr, uint16_t file_idx);

////////// VirFat ///////////////////////////////////

//////////////////////////////////////////////////////////////
//  User-defined settings, variables and callbacks of FAT ////
//////////////////////////////////////////////////////////////
//#define VIRFAT_READONLY // (#define / not define)
//#define VIRFAT_TIME_CALLBACK // (#define / not define)
//#define VIRFAT_DATE_DD_MM_YYYY	23, 5, 2025 //date of create / last access. If not defined - AUTO
//#define VIRFAT_TIME_HH_MM_SS	0, 0, 0 //time of create / last access. If not defined - AUTO
//#define VIRFAT_VOLID		0xFC561629 //disk UUID 
//#define VIRFAT_VOLNAME	"VIRTUAL_FAT" //8.3->11 chars. Disk Label (sometimes ignored by OS)
#define VIRFAT_VOLNAME		"MailNot_cfg"
//optional setings
#define VIRFAT_JMPBOOT		{0xEB, 0x3C, 0x90} //I dont know why but this data is important...
#define VIRFAT_OEMNAME		"virfat  " //8 chars. FAT internal name of program created FS

//////////////////////////////////////////////////////////////
//  Static data ////
//////////////////////////////////////////////////////////////
#ifdef MSD_VIRFAT_IMPL
volatile uint8_t virfat_enable = 1;


const char rules_file[] =
"# /etc/udev/rules.d/98-cokp_devices.rules\n"
"\n"
"ATTRS{manufacturer}==\"COKPOWEHEU\" ENV{CONNECTED_COKP}=\"yes\"\n"
"ENV{CONNECTED_COKP}==\"yes\", SUBSYSTEM==\"tty\", ATTRS{interface}==\"?*\", PROGRAM=\"/bin/bash -c \\\"ls /dev | grep tty_$attr{interface}_ | wc -l \\\"\", SYMLINK+=\"tty_$attr{interface}_%c\"\n"
"ENV{CONNECTED_COKP}==\"yes\", SUBSYSTEM==\"leds\", PROGRAM=\"/bin/bash -c 'chgrp -R dialout /sys/%p/ && chmod 660 /sys/%p/brightness'\""
;

const char lufa_driver_file[] =
";************************************************************\r\n"
"; Windows USB CDC ACM Setup File\r\n"
"; Copyright (c) 2000 Microsoft Corporation\r\n"
";************************************************************\r\n"
"\r\n"
"[DefaultInstall]\r\n"
"CopyINF=\"LUFA_CDC.INF\"\r\n"
"\r\n"
"[Version]\r\n"
"Signature=\"$Windows NT$\"\r\n"
"Class=Ports\r\n"
"ClassGuid={4D36E978-E325-11CE-BFC1-08002BE10318}\r\n"
"Provider=%MFGNAME%\r\n"
"DriverVer=7/1/2012,10.0.0.0\r\n"
"\r\n"
"[Manufacturer]\r\n"
"%MFGNAME%=DeviceList, NTx86, NTamd64, NTia64\r\n"
"\r\n"
"[SourceDisksNames]\r\n"
"\r\n"
"[SourceDisksFiles]\r\n"
"\r\n"
"[DestinationDirs]\r\n"
"DefaultDestDir=12\r\n"
"\r\n"
"[DriverInstall]\r\n"
"Include=mdmcpq.inf\r\n"
"CopyFiles=FakeModemCopyFileSection\r\n"
"AddReg=DriverInstall.AddReg\r\n"
"\r\n"
"[DriverInstall.Services]\r\n"
"Include=mdmcpq.inf\r\n"
"AddService=usbser, 0x00000002, LowerFilter_Service_Inst\r\n"
"\r\n"
"[DriverInstall.AddReg]\r\n"
"HKR,,EnumPropPages32,,\"msports.dll,SerialPortPropPageProvider\"\r\n"
"\r\n"
";------------------------------------------------------------------------------\r\n"
";  Vendor and Product ID Definitions\r\n"
";------------------------------------------------------------------------------\r\n"
"; When developing your USB device, the VID and PID used in the PC side\r\n"
"; application program and the firmware on the microcontroller must match.\r\n"
"; Modify the below line to use your VID and PID.  Use the format as shown below.\r\n"
"; Note: One INF file can be used for multiple devices with different VID and PIDs.\r\n"
"; For each supported device, append \",USB\\VID_xxxx&PID_yyyy\" to the end of the line.\r\n"
";------------------------------------------------------------------------------\r\n"
"[DeviceList]\r\n"
"%DESCRIPTION%=DriverInstall, USB\\VID_16C0&PID_05DF\r\n"
"\r\n"
"[DeviceList.NTx86]\r\n"
"%DESCRIPTION%=DriverInstall, USB\\VID_16C0&PID_05DF\r\n"
"\r\n"
"[DeviceList.NTamd64]\r\n"
"%DESCRIPTION%=DriverInstall, USB\\VID_16C0&PID_05DF\r\n"
"\r\n"
"[DeviceList.NTia64]\r\n"
"%DESCRIPTION%=DriverInstall, USB\\VID_16C0&PID_05DF\r\n"
"\r\n"
";------------------------------------------------------------------------------\r\n"
";  String Definitions\r\n"
";------------------------------------------------------------------------------\r\n"
";Modify these strings to customize your device\r\n"
";------------------------------------------------------------------------------\r\n"
"[Strings]\r\n"
"MFGNAME=\"http://www.lufa-lib.org\"\r\n"
"DESCRIPTION=\"LUFA CDC-ACM Virtual Serial Port\"\r\n"
;

const char readme_file[] =
"Индикатор каких-нибудь событий\r\n"
"(Сделан по мотивам RISO KAGAKU Webmail Notifier, https://github.com/bohnelang/Thunderbird_Add-On_USB_LED_Dream_Cheeky)\r\n"
"\r\n"
"В данную версию добавлен ЖК-экранчик на 2 строки по 16 символов чтобы можно было не просто мигать разными цветами, но и выводить текстовую информацию (просто посылая ее на соответствующий COM-порт). Ну и заодно пищалка (активируется посылкой стандартного \\b).\r\n"
"Все параметры, которые только получилось, снабжены настройками. Но учитывая ограниченный размер памяти примененного контроллера, при их изменении следует соблюдать осторожность. Все настройки отображаются в виде соответствующих .cfg - файлов. Их размер изначально 512 байт и не может быть увеличен (уменьшен - запросто). Поэтому если введенные вами значения увеличивают размер файла, удалите какие-нибудь пробелы (особенно много их в конце). Все настройки чувствительны к регистру. Кодировка всех файлов UTF-8 (вообще-то, это очевидно... но на windows иногда возникают проблемы - она пытается открыть их в UTF-16. Как с этим бороться со стороны устройства, я не знаю).\r\n"
"\r\n"
"Чтобы изменить настройки, достаточно вписать новые значения в соответствующие файлы, отмонтировать диск (ограничение съемных носителей: система может закешировать изменения и не записать их сразу), после чего нажать и удерживать кнопку SETUP. Как и при коротком нажатии, устройство переподключится в рабочем режиме (без \"флешки\"). Но при коротком нажатии изменения будут потеряны, а при длинном - сохранены.\r\n"
"\r\n"
"Устройство представляется в системе как Custom-HID (как и оригинальный MAIL NOTIFER) плюс COM-порт плюс, опционально, флеш-накопиталь.\r\n"
"\r\n"
"Световая индикация\r\n"
"По большей части сделано аналогично первоисточнику. Но добавлены настройки яркости отдельных цветов (файл RGB_CFG.CFG) и соответствие цветов их кодам. Дело в том, что \"родной\" софт посылает не RGB-код, а именно код цвета. И он, похоже, может отличаться. В общем, если что, можно подправить.\r\n"
"Помимо \"родного\" софта, можно напрямую писать в /sys/class/leds/riso_kagaku3\\:blue/brightness и подобные. По умолчанию, от рута, но правилом udev (см. ниже) это можно исправить.\r\n"
"\r\n"
"Текстовый дисплей\r\n"
"Построен на стандартном контроллере HD44780 и имеет некоторые ограничения. Главное из них - жестко зашитая таблица символов, в которой далеко не всегда встречаются русские буквы. Частично это можно обойти созданием до восьми пользовательских символов (файлы USRSYM_x.CFG). Также можно поменять у части символов соответствие стандартного UTF-32 кода значению в таблице знакогенератора (файлы USRTBL_x.CFG). Таких замен может быть до 64, хотя вряд ли в таблице вообще найдется достаточное количество похожих символов. Список доступных символов можно посмотреть в файле LCD_HELP.TXT\r\n"
"Еще одно ограничение дисплея - отсутствие автоматической настройки контраста. Поэтому добавлен файл LCD_CFG.TXT, в котором контрастность и яркость можно настроить вручную. Также там можно настроить форму курсора и реакцию на перевод строки. Курсор может выглядеть как большой мигающий блок, подчеркивание, оба или ничего. Другой способ изменить форму курсора - ESC-последовательности, описанные в LCD_HELP.TXT.\r\n"
"Настройка реакции на перевод строки обусловлена тем, что разные программы используют для этого разные последовательности. Некоторые используют \\r только для возврата каретки, другие для перевода строки. А в некоторых случаях перевод строки стандартными способами вообще не нужен.\r\n"
"\r\n"
"COM-порт\r\n"
"В системе может быть несколько виртуальных COM-портов, поэтому удобно назначить им псевдонимы (символьные ссылки) и изменить права доступа. Чтобы это сделать, создайте в /etc/udev/rules.d/ файл с каким-нибудь именем вроде 99-mailnotifer.rules и скопируйте содержимое файла RULES.TXT. После переподключения устройства появится файл /dev/tty_MAIL_LCD_0, в который можно посылать данные. Также этот файл переназначит \"файлам\" яркости группу на dialout и разрешит запись. В некоторых системах группы dialout нет, тогда замените на какую-нибудь другую. На uucp, например.\r\n"
"В windows способа назначить устройствам осмысленные имена, кажется, не существует. Более того, там даже не всегда автоматически устанавливаются драйвера на стандартные USB-устройства. Если это произошло, можно указать установку из LUFA_CDC.INF.\r\n"
"\r\n"
"\r\n"
"Автор: COKPOWEHEU (https://github.com/COKPOWEHEU/MailNotifer_ch32)\r\n"
;

void file_read(uint8_t *buf, uint32_t addr, uint16_t file_idx){
  const char *txt = (void*)0;
  int sz = 0;
  if(file_idx == 0){
    txt = rules_file;
    sz = sizeof(rules_file);
  }else if(file_idx == 1){
    txt = lufa_driver_file;
    sz = sizeof(lufa_driver_file);
  }else if(file_idx == 2){
    txt = readme_file;
    sz = sizeof(readme_file);
  }
  addr *= 512;
  int en = 512;
  if(addr + 512 > sz)en = (sz - addr);
  for(int i=0; i<en; i++)buf[i] = txt[addr + i];
  for(int i=en; i<512; i++)buf[i] = ' ';
}

void vf_snd_read(uint8_t *buf, uint32_t addr, uint16_t file_idx);
void vf_snd_write(uint8_t *buf, uint32_t addr, uint16_t file_idx);

const virfat_file_t virfat_rootdir[] = {
  {
    .name = "RULES   TXT",
    .file_read = file_read,
    .file_write = virfat_file_dummy,
    .size = (sizeof(rules_file)+511) / 512,
  },
  {
    .name = "LUFA_CDCINF",
    .file_read = file_read,
    .file_write = virfat_file_dummy,
    .size = (sizeof(lufa_driver_file)+511)/512,
  },
  {
    .name = "README  TXT",
    .file_read = file_read,
    .file_write = virfat_file_dummy,
    .size = (sizeof(readme_file)+511)/512,
  },
  {
    .name = "SND_CFG CFG",
    .file_read = vf_snd_read,
    .file_write = vf_snd_write,
    .size = 1,
  },
  
  rgb_virfat_files
  lcd_cfg_virfat_files
  
};

#endif

#endif
