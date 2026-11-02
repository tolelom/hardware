# RAM 실행용 빌드. 산출물은 build/ 아래에 모인다.
#
#   make          uf2 까지 만든다
#   make check    산출물이 실행 가능한 모양인지 확인한다
#   make dump     역어셈블을 본다
#   make load     보드에 올리고 실행한다 (BOOTSEL 모드여야 한다)
#   make clean    산출물을 지운다

CROSS   := arm-none-eabi-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump

TARGET   := firmware
BUILD    := build
LDSCRIPT := ld/ram.ld

# 부트롬이 이 주소에 적재하고 실행한다. 플래시가 아니라 SRAM 이다.
LOADADDR := 0x20000000

ARCH    := -mcpu=cortex-m0plus -mthumb
# nostdlib: 표준 라이브러리를 링크하지 않는다
# ffreestanding: main 이 특별하다는 가정 등 호스트 전제를 끈다
# gc-sections 짝: 쓰이지 않는 함수를 섹션 단위로 버리기 위해 나눠 둔다
CFLAGS  := $(ARCH) -nostdlib -ffreestanding -Og -g -Wall -Wextra \
           -ffunction-sections -fdata-sections
LDFLAGS := --gc-sections

SRCS := $(wildcard src/*.S) $(wildcard src/*.c)
OBJS := $(patsubst src/%,$(BUILD)/%.o,$(SRCS))

ELF := $(BUILD)/$(TARGET).elf
BIN := $(BUILD)/$(TARGET).bin
UF2 := $(BUILD)/$(TARGET).uf2

.PHONY: all check dump load clean
all: $(UF2)

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.S.o: src/%.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.c.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# 링커스크립트가 바뀌어도 다시 링크하도록 의존에 넣는다
$(ELF): $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) $(OBJS) -o $@

# ELF 는 심볼과 디버그 정보가 든 컨테이너다. 칩에 올라가는 것은 바이트뿐이다
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(UF2): $(BIN)
	picotool uf2 convert $< -t bin $@ -t uf2 -o $(LOADADDR)

check: $(BIN)
	@echo "크기      $$(stat -c%s $(BIN)) 바이트"
	@echo "벡터 0번  0x$$(xxd -s 0 -l 4 -e -g 4 $(BIN) | awk '{print $$2}')  (스택 꼭대기)"
	@echo "벡터 1번  0x$$(xxd -s 4 -l 4 -e -g 4 $(BIN) | awk '{print $$2}')  (리셋 핸들러, 홀수여야 한다)"
	@$(CROSS)nm $(ELF) | grep -E ' (_start|__stack_top)$$' | sed 's/^/심볼      /'

dump: $(ELF)
	$(OBJDUMP) -d $<

# -x: 올린 뒤 실행까지 한다. 빼면 적재만 하고 부트로더에 머문다
load: $(UF2)
	picotool load -x $(UF2)

clean:
	rm -rf $(BUILD)
