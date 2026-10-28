"""Daisy Seed3 핀 능력 표와 멀티 이펙터 플랫폼 핀 배정. 하드웨어 플랫폼 설계서 5.3절.

핀 표는 Seed3 데이터시트 v2.1.0 표 2(Pin Functions)에서 옮겼다. 기능은 주 기능과 대체 기능 1·2를 합친 것이다.
D29·D30은 USB_HS D-/D+라 USB 호스트 자리로 비워 둔다.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Pin:
    name: str
    physical: int
    stm32: str
    adc: str | None
    functions: frozenset[str]
    reserved: str | None = None


def _pin(name: str, physical: int, stm32: str, adc: str | None, functions: str, reserved: str | None = None) -> Pin:
    return Pin(name, physical, stm32, adc, frozenset(f.strip() for f in functions.split("/") if f.strip()), reserved)


# Seed3 데이터시트 표 2
SEED3_PINS = {p.name: p for p in [
    _pin("D0", 1, "PB12", None, "USB_HS_ID/UART5_RX/USART3_CK/TIM1_BKIN"),
    _pin("D1", 2, "PC11", None, "SDMMC1_D3/USART3_RX/UART4_RX/SPI3_MISO/I2S3_SDI/HRTIM_FLT2"),
    _pin("D2", 3, "PC10", None, "SDMMC1_D2/USART3_TX/UART4_TX/SPI3_SCK/I2S3_CK/HRTIM_EEV1"),
    _pin("D3", 4, "PC9", None, "SDMMC1_D1/UART5_CTS/I2S_CKIN/MCO2"),
    _pin("D4", 5, "PC8", None, "SDMMC1_D0/UART5_RTS"),
    _pin("D5", 6, "PD2", None, "SDMMC1_CMD/UART5_RX"),
    _pin("D6", 7, "PC12", None, "SDMMC1_CK/UART5_TX/USART3_CK/SPI3_MOSI/I2S3_SDO"),
    _pin("D7", 8, "PG10", None, "SPI1_NSS/I2S1_WS/HRTIM_FLT5"),
    _pin("D8", 9, "PG11", None, "SPI1_SCK/I2S1_CK/LPTIM1_IN2/HRTIM_EEV4"),
    _pin("D9", 10, "PB4", None, "SPI1_MISO/UART7_TX/I2S1_SDI/SPI3_MISO/I2S3_SDI/SPI6_MISO"),
    _pin("D10", 11, "PB5", None, "SPI1_MOSI/UART5_RX/I2S1_SDO/SPI3_MOSI/I2S3_SDO/SPI6_MOSI/I2C4_SMBA/TIM17_BKIN"),
    _pin("D11", 12, "PB8", None, "I2C1_SCL/UART4_RX/I2C4_SCL/TIM16_CH1/TIM4_CH3"),
    _pin("D12", 13, "PB9", None, "I2C1_SDA/UART4_TX/SPI2_NSS/I2S2_WS/I2C4_SDA/I2C4_SMBA/TIM17_CH1/TIM4_CH4"),
    _pin("D13", 14, "PB6", None, "USART1_TX/LPUART1_TX/UART5_TX/I2C1_SCL/I2C4_SCL/TIM16_CH1N/TIM4_CH1"),
    _pin("D14", 15, "PB7", None, "USART1_RX/LPUART1_RX/I2C1_SDA/I2C4_SDA/TIM17_CH1N/TIM4_CH2"),
    _pin("D15", 22, "PC0", "A0", "ADC0/SAI2_FS_B"),
    _pin("D16", 23, "PA3", "A1", "ADC1/USART2_RX/TIM2_CH4/TIM5_CH4"),
    _pin("D17", 24, "PB1", "A2", "ADC2/TIM1_CH3N/TIM3_CH4"),
    _pin("D18", 25, "PA7", "A3", "ADC3/SPI1_MOSI/I2S1_SDO/SPI6_MOSI/TIM1_CH1N/TIM3_CH2"),
    _pin("D19", 26, "PA6", "A4", "ADC4/SPI1_MISO/I2S1_SDI/SPI6_MISO/TIM1_BKIN/TIM3_CH1"),
    _pin("D20", 27, "PC1", "A5", "ADC5"),
    _pin("D21", 28, "PC4", "A6", "ADC6/I2S1_MCK"),
    _pin("D22", 29, "PA5", "A7", "ADC7/DAC1_OUT2/SPI1_SCK/I2S1_CK/SPI6_SCK/D2PWREN/TIM2_CH1"),
    _pin("D23", 30, "PA4", "A8", "ADC8/DAC1_OUT1/SPI1_NSS/I2S1_WS/SPI3_NSS/I2S3_WS/SPI6_NSS/D1PWREN"),
    _pin("D24", 31, "PA1", "A9", "ADC9/SAI2_MCLK_B/UART4_RX/TIM2_CH2/TIM5_CH2"),
    _pin("D25", 32, "PA0", "A10", "ADC10/SAI2_SD_B/UART4_TX/TIM2_CH1/TIM2_ETR/TIM5_CH1"),
    _pin("D26", 33, "PD11", None, "SAI2_SD_A/I2C4_SMBA/LPTIM2_IN2"),
    _pin("D27", 34, "PG9", None, "SAI2_FS_B/USART6_RX/SPI1_MISO/I2S1_SDI"),
    _pin("D28", 35, "PA2", "A11", "ADC11/SAI2_SCK_B/USART2_TX/TIM2_CH3/TIM5_CH3"),
    _pin("D29", 36, "PB14", None, "USB_HS_D-/USART1_TX/TIM1_CH2N", reserved="USB_HS D-"),
    _pin("D30", 37, "PB15", None, "USB_HS_D+/USART1_RX", reserved="USB_HS D+"),
]}

# 기능 이름 → 핀. 주변장치 기능은 이름 앞부분(SPI1_, I2C1_, USART1_, SDMMC1_)이 그 핀의 기능 목록에 있어야 한다
PLATFORM = {
    "K1_DRIVE": "D0",
    "SDMMC1_D3": "D1", "SDMMC1_D2": "D2", "SDMMC1_D1": "D3", "SDMMC1_D0": "D4",
    "SDMMC1_CMD": "D5", "SDMMC1_CK": "D6",
    "SPI1_NSS": "D7", "SPI1_SCK": "D8", "SPI1_MISO": "D9", "SPI1_MOSI": "D10",
    "I2C1_SCL": "D11", "I2C1_SDA": "D12",
    "USART1_TX": "D13", "USART1_RX": "D14",
    "EXP_ADC": "D15",
    "DISPLAY_BL_PWM": "D16",
    "DISPLAY_DC": "D17", "DISPLAY_RST": "D18",
    "MCP_INT_A": "D19",
    "VMON_ADC": "D20",
    "MCP_INT_B": "D21",
    "FS1": "D22", "FS2": "D23", "FS3": "D24",
    "LED1": "D25", "LED2": "D26", "LED3": "D27",
}
PERIPHERAL_PREFIXES = ("SPI1_", "I2C1_", "USART1_", "SDMMC1_")


def check(assignment: dict[str, str]) -> list[str]:
    problems = []
    by_pin: dict[str, list[str]] = {}
    for function, name in assignment.items():
        by_pin.setdefault(name, []).append(function)
        pin = SEED3_PINS.get(name)
        if pin is None:
            problems.append(f"{function}: 없는 핀 {name}")
            continue
        if pin.reserved:
            problems.append(f"{function}: {name}은 {pin.reserved} 예약 핀")
        if function.endswith("_ADC") and pin.adc is None:
            problems.append(f"{function}: {name}은 ADC 핀이 아님")
        if function.startswith(PERIPHERAL_PREFIXES) and function not in pin.functions:
            problems.append(f"{function}: {name}에 그 기능이 없음")
    for name, functions in by_pin.items():
        if len(functions) > 1:
            problems.append(f"핀 중복 {name}: {', '.join(sorted(functions))}")
    return problems


def net(function: str) -> str:
    return f"SEED_{PLATFORM[function]}_{function}"
