# SRAM 실행용 빌드. 산출물은 build/ 아래에 모인다.
#
#   make          uf2까지 만든다
#   make check    uf2가 SRAM용으로 만들어졌는지 확인한다
#   make dump     역어셈블을 본다
#   make load     보드에 올린다 (BOOTSEL 모드여야 한다)
#   make clean    산출물을 지운다

CROSS    := arm-none-eabi-
CC       := $(CROSS)gcc
LD       := $(CROSS)ld
OBJDUMP  := $(CROSS)objdump
NM       := $(CROSS)nm
PICOTOOL := picotool

TARGET   := firmware
BUILD    := build
LDSCRIPT := ld/sram.ld

ARCH    := -mcpu=cortex-m0plus -mthumb
# nostdlib: 표준 라이브러리를 링크하지 않는다
# ffreestanding: 호스트 OS가 있다는 가정을 끈다
# function-sections, data-sections: --gc-sections가 함수·변수 단위로 버릴 수 있게 나눈다
CFLAGS  := $(ARCH) -nostdlib -ffreestanding -Og -g -Wall -Wextra \
           -ffunction-sections -fdata-sections
LDFLAGS := --gc-sections

SRCS := $(wildcard src/*.S) $(wildcard src/*.c)
OBJS := $(patsubst src/%,$(BUILD)/%.o,$(SRCS))

ELF := $(BUILD)/$(TARGET).elf
UF2 := $(BUILD)/$(TARGET).uf2

.PHONY: all check dump load clean
all: $(UF2)

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.S.o: src/%.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.c.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# 링커 스크립트가 바뀌어도 다시 링크한다
$(ELF): $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) $(OBJS) -o $@

# ELF에서 바로 만든다. 적재 주소를 ELF가 갖고 있어 여기서 주소를 다시 적지 않는다.
# bin에서 만들면 -o를 빠뜨렸을 때 기본값 0x10000000(flash)으로 들어간다.
$(UF2): $(ELF)
	$(PICOTOOL) uf2 convert $< $@

# 데이터시트 2.8.4.2: RAM 전용 바이너리는 0x20000000~0x20042000 범위에 있어야 하고,
# 가장 낮은 블록 주소로 점프한다. 그 주소가 _start여야 한다.
check: $(UF2)
	@python3 -I tools/check_uf2.py $(UF2) $$($(NM) $(ELF) | awk '$$3 == "_start" {print $$1}')

dump: $(ELF)
	$(OBJDUMP) -d $<

load: $(UF2)
	$(PICOTOOL) load -x $(UF2)

clean:
	rm -rf $(BUILD)
