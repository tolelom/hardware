# Daisy Seed3 펌웨어. libDaisy 표준 Makefile을 그대로 쓴다.
#   make            빌드 (build/effector.bin)
#   make program-dfu  USB DFU 업로드 (콘솔에서 dfu를 입력하거나 BOOT+RESET으로 DFU 모드에 둔 뒤)
TARGET = effector

CPP_SOURCES = src/main.cpp $(wildcard lib/effector_core/src/*.cpp)
C_INCLUDES = -Ilib/effector_core/src
CPP_STANDARD = -std=gnu++17

# newlib-nano는 기본으로 %f를 빼므로 status 출력과 명령 숫자 파싱에 필요하다
LDFLAGS = -u _printf_float -u _scanf_float

LIBDAISY_DIR = lib/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
