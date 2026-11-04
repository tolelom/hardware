# flash 부팅 빌드. 산출물은 이 폴더의 build/ 아래에 모인다.
#
#   make          uf2까지 만든다
#   make check    uf2가 flash 부팅 조건을 만족하는지 확인한다
#   make dump     역어셈블을 본다
#   make load     보드 flash에 쓴다 (BOOTSEL 모드여야 한다. 공장 펌웨어를 덮어쓴다)
#   make bootsel  usb_cdc.c가 돌고 있을 때 1200 baud 설정으로 BOOTSEL 모드로 재부팅한다
#   make clean    산출물을 지운다
#
#   MAIN=<파일>   main.c 대신 그 파일로 빌드한다. 예: make MAIN=clock.c load

CROSS    := arm-none-eabi-
CC       := $(CROSS)gcc
LD       := $(CROSS)ld
OBJDUMP  := $(CROSS)objdump
NM       := $(CROSS)nm
PICOTOOL := picotool
CDC_PORT ?= /dev/serial/by-id/usb-tolelom_RP2040_0to1_CDC_0001-if00

MAIN     ?= main.c
BUILD    := build/$(basename $(MAIN))
ELF      := $(BUILD)/firmware.elf
UF2      := $(BUILD)/firmware.uf2
LDSCRIPT := flash.ld

ARCH    := -mcpu=cortex-m0plus -mthumb
CFLAGS  := $(ARCH) -nostdlib -ffreestanding -Og -g -Wall -Wextra \
           -ffunction-sections -fdata-sections
LDFLAGS := --gc-sections

OBJS := $(BUILD)/boot2.S.o $(BUILD)/start.S.o $(BUILD)/$(MAIN).o

.PHONY: all check dump load bootsel clean
all: $(UF2)

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.S.o: %.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.c.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) $(OBJS) -o $@

$(UF2): $(ELF)
	$(PICOTOOL) uf2 convert $< $@

check: $(UF2)
	@python3 -I check_flash_uf2.py $(UF2) $$($(NM) $(ELF) | awk '$$3 == "__image_end" {print $$1}')

dump: $(ELF)
	$(OBJDUMP) -d $<

load: $(UF2)
	$(PICOTOOL) load -x $(UF2)

bootsel:
	stty -F $(CDC_PORT) 1200

clean:
	rm -rf build
