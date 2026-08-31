/* 1 단계: GPIO17 (빨강 LED) 켜기.
   LED 는 active-low 다. 핀을 LOW 로 만들어야 켜진다.
   주소 출처는 RP2040 데이터시트 2.14 Subsystem Resets 와 2.19 GPIO. */

/* 하드웨어 레지스터는 반드시 volatile 이어야 한다.
   빼면 컴파일러가 중복으로 보이는 쓰기를 지운다. */
#define REG(addr) (*(volatile unsigned int *)(addr))

#define RESETS_BASE     0x4000c000u
#define IO_BANK0_BASE   0x40014000u
#define SIO_BASE        0xd0000000u

#define RESETS_RESET         (RESETS_BASE   + 0x00)
#define RESETS_RESET_DONE    (RESETS_BASE   + 0x08)
#define IO_BANK0_GPIO17_CTRL (IO_BANK0_BASE + 0x8c)   /* 0x04 + 17*8 */
#define SIO_GPIO_OE_SET      (SIO_BASE      + 0x24)
#define SIO_GPIO_OUT_SET     (SIO_BASE      + 0x14)
#define SIO_GPIO_OUT_CLR     (SIO_BASE      + 0x18)
#define SIO_GPIO_OUT_XOR     (SIO_BASE      + 0x1c)

#define RESETS_IO_BANK0   (1u << 5)   /* RESETS 안에서 IO_BANK0 의 자리 */
#define FUNCSEL_SIO       5u          /* 데이터시트 Table 279 의 F5 */
#define LED_R             17u

int main(void)
{
    /* 1. IO_BANK0 의 잠금을 푼다. 비트 5 만 0 으로 하고 나머지는 보존한다.
          이 레지스터에는 SET/CLR 짝이 없어 읽고 고쳐 쓰는 수밖에 없다. */
    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_IO_BANK0;

    /* 2. 해제가 끝날 때까지 기다린다.
          RESET_DONE 의 비트 5 가 1 이 되면 완료다.
          안 기다리면 아래 설정이 날아갈 수 있다. */
    while ((REG(RESETS_RESET_DONE) & RESETS_IO_BANK0) == 0) { }


    /* 3. GPIO17 을 SIO 가 쓰도록 배정한다.
          FUNCSEL 은 CTRL 의 하위 5 비트이고, 나머지 필드는 리셋값으로 둔다. */
    REG(IO_BANK0_GPIO17_CTRL) = FUNCSEL_SIO;


    /* 4. 출력 방향을 켠다. 안 켜면 값을 써도 핀이 안 움직인다. */
    REG(SIO_GPIO_OE_SET) = 1u << LED_R;


    /* 5. LOW 로 만들어 LED 를 켠다. active-low 라 CLR 이 켜는 쪽이다. */
    REG(SIO_GPIO_OUT_CLR) = 1u << LED_R;

    /* 6. 깜빡인다.
          리셋 직후 모든 핀은 입력이고 풀다운이 켜져 있어(PADS 리셋값 0x56 의 PDE)
          세 LED 가 희미하게 켜진 상태다. 정지 상태로는 그것과 구별되지 않으므로
          깜빡임으로 판정한다.
          OUT_XOR 은 현재 값을 읽지 않고 뒤집는다.
          지연 루프의 volatile 을 빼면 컴파일러가 루프를 통째로 지운다. */
    while (1) {
        REG(SIO_GPIO_OUT_XOR) = 1u << LED_R;
        for (volatile int i = 0; i < 100000; i++) { }
    }
}
