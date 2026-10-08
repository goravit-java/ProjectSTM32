#include "Drivers/uart_driver.h"
#include "Drivers/cortex_driver.h"

#define USART_SR_TXE        (1U << 7U)
#define USART_CR1_RE        (1U << 2U)
#define USART_CR1_TE        (1U << 3U)
#define USART_CR1_TXEIE     (1U << 7U)
#define USART_CR1_UE        (1U << 13U)

#define UART_TX_PIN         2U        /* PA2 = USART2_TX */
#define UART_RX_PIN         3U        /* PA3 = USART2_RX */
#define GPIO_MODE_ALTERNATE 0x2U
#define GPIO_AF7_USART2     0x7U
#define USART2_BRR_115200   0x008AU   /* 16 MHz / 115200 = 8.68 -> Mantissa 8, Fraction 10 */

#define UART_TX_INDEX_MASK  (UART_TX_BUFFER_SIZE - 1U)
#define UART_UINT_MAX_DIGITS 10U      /* uint32_t มีได้สูงสุด 10 หลัก (4294967295) */
#define DECIMAL_BASE        10U
#define DECIMAL_BASE_S      10        /* ฐาน 10 แบบ Signed สำหรับคำนวณทศนิยม (int32_t) */
#define ROUND_HALF          0.5f
#define FLOAT_DECIMAL_SCALE 10.0f     /* ทศนิยม 1 ตำแหน่ง */

/* Ring Buffer สำหรับการส่ง
 * - tx_head: เขียนโดยโปรแกรมหลักเท่านั้น (ตำแหน่งที่จะใส่ข้อมูลถัดไป)
 * - tx_tail: เขียนโดย ISR เท่านั้น (ตำแหน่งที่จะส่งออกถัดไป)
 * แต่ละตัวมีผู้เขียนเพียงฝั่งเดียว และการอ่าน/เขียน uint16_t บน Cortex-M4 เป็น Atomic
 * จึงไม่ต้องปิด Interrupt ระหว่างใช้งาน (Single-Producer / Single-Consumer)
 */
static volatile uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t tx_head = 0U;
static volatile uint16_t tx_tail = 0U;

void USART2_IRQHandler(void);

void UART2_Init(void) {
    uint32_t tx_shift_mode = UART_TX_PIN * GPIO_MODER_BITS_PER_PIN;
    uint32_t rx_shift_mode = UART_RX_PIN * GPIO_MODER_BITS_PER_PIN;
    uint32_t tx_shift_af = UART_TX_PIN * GPIO_AFR_BITS_PER_PIN;
    uint32_t rx_shift_af = UART_RX_PIN * GPIO_AFR_BITS_PER_PIN;

    /* 1. เปิด Clock ให้ GPIOA และ USART2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* 2. ตั้งค่า PA2 (TX) และ PA3 (RX) เป็น Alternate Function 7 (AF07) */
    GPIOA->MODER &= ~((GPIO_MODER_FIELD_MASK << tx_shift_mode) | (GPIO_MODER_FIELD_MASK << rx_shift_mode));
    GPIOA->MODER |= ((GPIO_MODE_ALTERNATE << tx_shift_mode) | (GPIO_MODE_ALTERNATE << rx_shift_mode));

    GPIOA->AFR[0U] &= ~((GPIO_AFR_FIELD_MASK << tx_shift_af) | (GPIO_AFR_FIELD_MASK << rx_shift_af));
    GPIOA->AFR[0U] |= ((GPIO_AF7_USART2 << tx_shift_af) | (GPIO_AF7_USART2 << rx_shift_af));

    /* 3. ตั้งค่า Baud Rate = 115200 (คำนวณจาก Clock HSI 16 MHz) */
    USART2->BRR = USART2_BRR_115200;

    tx_head = 0U;
    tx_tail = 0U;

    /* 4. เปิดใช้งาน Transmitter, Receiver และ USART2 (TXEIE ยังปิดไว้ จนกว่าจะมีข้อมูลรอส่ง) */
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);

    /* 5. เปิด USART2 Interrupt ใน NVIC */
    Cortex_NvicEnableIrq((uint8_t)USART2_IRQN);
}

/* ใส่ 1 ตัวอักษรลง Buffer แล้วคืนค่าทันที (ไม่รอ Hardware) ถ้า Buffer เต็มจะทิ้งตัวอักษรนั้น */
void UART2_SendChar(char c) {
    uint16_t next = (uint16_t)(((uint32_t)tx_head + 1U) & UART_TX_INDEX_MASK);

    if (next != tx_tail) {
        tx_buffer[tx_head] = (uint8_t)c;
        tx_head = next;
        USART2->CR1 |= USART_CR1_TXEIE; /* ปลุก ISR ให้เริ่ม/ส่งต่อ */
    } else {
        /* Buffer เต็ม: ทิ้งตัวอักษรนี้ (ข้อความชุดยาวสุดของระบบใช้ไม่ถึงครึ่ง Buffer) */
    }
}

/* TXE Interrupt: Data Register ว่างแล้ว -> ส่ง Byte ถัดไปจาก Buffer
 * ถ้า Buffer ว่าง ให้ปิด TXEIE ไม่เช่นนั้น TXE จะค้างเป็น 1 และ ISR จะถูกเรียกซ้ำไม่รู้จบ
 */
void USART2_IRQHandler(void) {
    if (((USART2->SR & USART_SR_TXE) != 0U) && ((USART2->CR1 & USART_CR1_TXEIE) != 0U)) {
        if (tx_tail != tx_head) {
            USART2->DR = (uint32_t)tx_buffer[tx_tail];
            tx_tail = (uint16_t)(((uint32_t)tx_tail + 1U) & UART_TX_INDEX_MASK);
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    } else {
        /* Interrupt จากสาเหตุอื่นที่ไม่ได้เปิดใช้งาน: ไม่ทำอะไร */
    }
}

void UART2_SendString(const char *str) {
    const char *p = str;

    while (*p != '\0') {
        UART2_SendChar(*p);
        p++;
    }
}

/* ส่งค่าจำนวนเต็มไม่ติดลบ (เช่น ราคา, Stock) โดยไม่ใช้ sprintf/printf
 * (หลีกเลี่ยงการพึ่งพา Standard I/O Library ซึ่งกินพื้นที่ Flash มากบนระบบ Bare-metal)
 */
void UART2_SendUint(uint32_t value) {
    char buf[UART_UINT_MAX_DIGITS];
    uint8_t i = 0U;
    uint32_t v = value;

    if (v == 0U) {
        UART2_SendChar('0');
    } else {
        while (v > 0U) {
            buf[i] = (char)((v % DECIMAL_BASE) + (uint32_t)'0');
            i++;
            v /= DECIMAL_BASE;
        }

        while (i > 0U) {
            i--;
            UART2_SendChar(buf[i]);
        }
    }
}

/* ส่งค่า float แบบทศนิยม 1 ตำแหน่ง โดยไม่มี Label/หน่วยต่อท้าย (เช่น "43.5")
 * ปัดเศษทศนิยมตำแหน่งที่ 1 อย่างถูกต้อง (รวมกรณีทด เช่น 29.96 -> "30.0" ไม่ใช่ "29.10")
 */
void UART2_SendFloat1(float value) {
    float v = value;
    int32_t integer_part;
    int32_t decimal_part;

    if (v < 0.0f) {
        UART2_SendChar('-');
        v = -v;
    } else {
        /* ค่าบวก: ไม่ต้องใส่เครื่องหมาย */
    }

    integer_part = (int32_t)v;
    decimal_part = (int32_t)(((v - (float)integer_part) * FLOAT_DECIMAL_SCALE) + ROUND_HALF);

    if (decimal_part >= DECIMAL_BASE_S) {
        decimal_part = 0;
        integer_part++;
    } else {
        /* ไม่มีการทดหลัก */
    }

    UART2_SendUint((uint32_t)integer_part);
    UART2_SendChar('.');
    UART2_SendUint((uint32_t)decimal_part);
}
