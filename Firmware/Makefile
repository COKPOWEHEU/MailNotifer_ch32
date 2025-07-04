progname := firmware
progdir := res
files := main usbd_lib usb_class tty usb_class_virfat strlib lcd_hd44780

MCU := CH32V203
#HSE_CLOCK := 8000000

SPEC_CFLAGS = -DMEMFUNC -DSTRMATCH_FUNC
#SPEC_CFLAGS = -DMEMFUNC_DMA=1,1,ch32v20x

srcdir := src
builddir := build
libdir := lib

#CROSS_COMPILE=riscv64-unknown-elf-
CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
SIZE=$(CROSS_COMPILE)size

ifneq (, $(shell which riscv64-unknown-elf-gcc 2>/dev/null))
  CROSS_COMPILE=riscv64-unknown-elf-
  GCCVER=`$(CC) --version | sed -n "1s/.* \([0-9]*[.][0-9]*[.][0-9]*\).*/\1/p"`
  GCCPATH = -L/usr/lib/gcc/riscv64-unknown-elf/$(GCCVER)/$(ARCH_$(MCU))/ilp32/
else ifneq (, $(shell which riscv64-linux-gnu-gcc 2>/dev/null))
  CROSS_COMPILE=riscv64-linux-gnu-
else
  $(error Unknown RISC-V compiler)
endif


COMMON_CH32V203 = -march=rv32imac_zicsr -DCH32V203G8
ARCH_CH32V203 = rv32imac
LIB_CH32V203 = $(libdir)/ch32v20x
LDFLAGS_CH32V203 = -T $(libdir)/ch32v20x/ch32v203.ld
#STARTUP_CH32V203 = startup_ch32v20x_D6
STARTUP_CH32V203 = startup_ch32v20x_D8

COMMON_CH32V208 = -march=rv32imac_zicsr -DCH32V20x_D8W
ARCH_CH32V208 = rv32imac
LIB_CH32V208 = $(libdir)/ch32v20x
LDFLAGS_CH32V208 = -T $(libdir)/ch32v20x/ch32v208.ld
STARTUP_CH32V208 = startup_ch32v20x_D8W

COMMON_CH32V303 = -march=rv32imac_zicsr -DCH32V30x_D8
ARCH_CH32V303 = rv32imac
LIB_CH32V303 = $(libdir)/ch32v30x
LDFLAGS_CH32V303 = -T $(libdir)/ch32v30x/ch32v303.ld
STARTUP_CH32V303 = startup_ch32v30x_D8

COMMON_CH32V307 = -march=rv32imafc -DCH32V30x_D8C
ARCH_CH32V307 = rv32imafc
LIB_CH32V307 = $(libdir)/ch32v30x
LDFLAGS_CH32V307 = -T $(libdir)/ch32v30x/ch32v307.ld
STARTUP_CH32V307 = startup_ch32v30x_D8C


COMMON = $(COMMON_$(MCU))
COMMON += -MD -MP -MT $(builddir)/$(*F).o -MF $(builddir)/dep/$(@F).mk
COMMON += -mabi=ilp32 -mcmodel=medany -static -fno-plt -fno-pic -fno-asynchronous-unwind-tables -fno-unwind-tables
COMMON += -DHSE_VALUE=$(HSE_CLOCK)
ASMFLAGS = $(COMMON)
CFLAGS = $(COMMON)
CFLAGS += $(SPEC_CFLAGS)
CFLAGS += -Wall -Os -g -Wno-main -Wstack-usage=400 -ffreestanding -Wno-unused -nostdlib -fno-builtin-printf
CFLAGS += -I$(libdir)
CFLAGS += -I$(LIB_$(MCU))/inc
CFLAGS += -I$(LIB_$(MCU))/inc/Peripheral/inc
LDFLAGS = $(COMMON)
LDFLAGS += $(LDFLAGS_$(MCU))
LDFLAGS += -Wl,--build-id=none -nostdlib
LDFLAGS += $(GCCPATH) -lgcc 
#LDFLAGS += /usr/lib/gcc/riscv64-unknown-elf/12.2.0/rv32imac/ilp32/libgcc.a

files += $(STARTUP_$(MCU))

frmname = $(progdir)/$(progname)
objs = $(addprefix $(builddir)/,$(addsuffix .o,$(files)))

all: $(frmname).bin $(frmname).hex $(frmname).lss size

$(frmname).bin: $(frmname).elf
	$(OBJCOPY) -O binary $^ $@
$(frmname).hex: $(frmname).elf
	$(OBJCOPY) -Oihex $(frmname).elf $(frmname).hex
$(frmname).lss: $(frmname).elf
	$(OBJDUMP) -D -S $(frmname).elf > $(frmname).lss

$(frmname).elf: $(objs) $(LSCRIPT)
	mkdir -p $(progdir)
	@ echo "..linking"
	$(LD) $(objs) $(LDFLAGS) -o $@ 

$(builddir)/%.o: $(srcdir)/%.c
	@echo -e "\nCompile " $<
	mkdir -p $(builddir)
	$(CC) -c $(CFLAGS) $< -o $@
	@echo
$(builddir)/%.o: $(srcdir)/%.S
	@echo -e "\nCompile " $<
	mkdir -p $(builddir)
	$(CC) -c $(ASMFLAGS) $< -o $@
	@echo
$(builddir)/%.o: $(libdir)/%.S
	@echo -e "\nCompile " $<
	mkdir -p $(builddir)
	$(CC) -c $(ASMFLAGS) $< -o $@
	@echo
$(builddir)/audiodata.o: $(srcdir)/audiodata.wav
	mkdir -p $(builddir)
	$(CROSS_COMPILE)ld -melf32lriscv -r -b binary -o $@ $<
	$(CROSS_COMPILE)objcopy --rename-section .data=.rodata,alloc,load,readonly,data,contents $@ $@
$(builddir)/usb_class_msd.o: $(builddir)/src_zip.h
$(builddir)/src_zip.h:
	mkdir -p $(builddir)
	@echo "Build src_zip.h"
	rm -f $(builddir)/src.zip
	zip -r $(builddir)/src.zip src > /dev/null
	zip $(builddir)/src.zip Makefile > /dev/null
	echo "const uint8_t src_zip[] = {" > $@
	hexdump -v -e '1/1 "0x%.2X,"' $(builddir)/src.zip >> $@
	#xxd -i $(builddir)/src.zip >> $@
	echo "};" >> $@
clean:
	rm -rf $(progdir)
	rm -rf $(builddir)
size: $(frmname).elf
	$(SIZE) $(frmname).elf
	
wch_isp = "/home/misha/prog/2_wch/wch-isp/wch-isp"
uart_port="/dev/tty_STFLASH_0"

prog:	$(frmname).bin size
	stty -F $(uart_port) 300
	stty -F $(uart_port) 50
	echo 'RBU' > $(uart_port)
	echo 'rBU' > $(uart_port)
	sleep 1
	$(wch_isp) --port=$(uart_port) 1000000 -p -b write $(frmname).bin
	stty -F $(uart_port) 50
	echo 'RbU' > $(uart_port)
	sleep 1
	echo 'rbuz' > $(uart_port)
test:	
	stty -F $(uart_port) 300
	stty -F $(uart_port) 50
	echo 'RBU' > $(uart_port)
	echo 'rBU' > $(uart_port)
	sleep 1
	$(wch_isp) --port=$(uart_port) 1000000 -p -b info
	stty -F $(uart_port) 50
	echo 'RbU' > $(uart_port)
	sleep 1
	echo 'rbuz' > $(uart_port)
reset:
	stty -F $(uart_port) 300
	stty -F $(uart_port) 50
	echo 'Rbu' > $(uart_port)
	echo 'rbuz' > $(uart_port)
	
pause:
	stty -F $(uart_port) 300
	stty -F $(uart_port) 50
	echo 'Rbu' > $(uart_port)
	sleep 5
	echo 'rbuz' > $(uart_port)
	
stflash:	$(frmname).bin
	stty -F /dev/tty_STFLASH_0 300
	stty -F /dev/tty_STFLASH_0 50
	$(wch_isp) --port=/dev/tty_STFLASH_0 --reset=RTS --boot0=DTR -b write $(frmname).bin
	stty -F /dev/tty_STFLASH_0 50
	echo 'rbuz' > /dev/tty_STFLASH_0
	
ft_rst:
	$(wch_isp) --port=/dev/ttyUSB0 --boot0=DTR --reset=RTS -b info
	
ftdi:	$(frmname).bin
	$(wch_isp) --port=/dev/ttyUSB0 --boot0=DTR --reset=RTS -b write $(frmname).bin
usb:	$(frmname).bin
	$(wch_isp) -b write $(frmname).bin

-include $(shell mkdir -p $(builddir)/dep) $(wildcard $(builddir)/dep/*)

.PHONY: all clean
