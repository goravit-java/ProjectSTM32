#include "Drivers/uart_driver.h"
#include "Drivers/cortex_driver.h"

#define USART_SR_TXE      (1U << 7U)
#define USART_CR1_RE      (1U << 2U)
#define USART_CR1_TE      (1U << 3U)
#define USART_CR1_TXEIE   (1U << 7U)
#define USART_CR1_UE      (1U << 13U)

#define UART_TX_INDEX_MASK  (UART_TX_BUFFER_SIZE - 1U)

/* Ring Buffer สำหรับการส่ง
 * - tx_head: เขียนโดยโปรแกรมหลักเท่านั้น (ตำแหน่งที่จะใส่ข้อมูลถัดไป)
 * - tx_tail: เขียนโดย ISR เท่านั้น (ตำแหน่งที่จะส่งออกถัดไป)
 * แต่ละตัวมีผู้เขียนเพียงฝั่งเดียว และการอ่าน/เขียน uint16_t บน Cortex-M4 เป็น Atomic
 * จึงไม่ต้องปิด Interrupt ระหว่างใช้งาน (Single-Producer / Single-Consumer)
 */
static volatile uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t tx_head = 0U;
static volatile uint16_t tx_tail = 0U;
static volatile uint32_t tx_dropped = 0U;

void USART2_IRQHandler(void);

void UART2_Init(void) {
    /* 1. เปิด Clock ให้ GPIOA และ USART2 */
    RCC->AHB1ENR |= (1U << 0U);          /* GPIOA Clock Enable */
    RCC->APB1ENR |= (1U << 17U);         /* USART2 Clock Enable */

    /* 2. ตั้งค่า PA2 (TX) และ PA3 (RX) เป็น Alternate Function 7 (AF07) */
    GPIOA->MODER &= ~((3U << (2U * 2U)) | (3U << (3U * 2U)));
    GPIOA->MODER |=  ((2U << (2U * 2U)) | (2U << (3U * 2U)));     /* Alternate Function mode */

    GPIOA->AFR[0] &= ~((0xFU << (2U * 4U)) | (0xFU << (3U * 4U)));
    GPIOA->AFR[0] |=  ((0x7U << (2U * 4U)) | (0x7U << (3U * 4U))); /* AF07 (USART2) */

    /* 3. ตั้งค่า Baud Rate = 115200 (คำนวณจาก Clock HSI 16 MHz) */
    USART2->BRR = 0x008AU;

    tx_head = 0U;
    tx_tail = 0U;
    tx_dropped = 0U;

    /* 4. เปิดใช้งาน Transmitter, Receiver และ USART2 (TXEIE ยังปิดไว้ จนกว่าจะมีข้อมูลรอส่ง) */
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);

    /* 5. เปิด USART2 Interrupt ใน NVIC */
    Cortex_NvicEnableIrq((uint8_t)USART2_IRQN);
}

/* ใส่ 1 ตัวอักษรลง Buffer แล้วคืนค่าทันที (ไม่รอ Hardware) ถ้า Buffer เต็มจะทิ้งตัวอักษรนั้นและนับไว้ */
void UART2_SendChar(char c) {
    uint16_t next = (uint16_t)(((uint32_t)tx_head + 1U) & UART_TX_INDEX_MASK);

    if (next != tx_tail) {
        tx_buffer[tx_head] = (uint8_t)c;
        tx_head = next;
        USART2->CR1 |= USART_CR1_TXEIE; /* ปลุก ISR ให้เริ่ม/ส่งต่อ */
    } else {
        tx_dropped++;
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
    }
}

uint32_t UART2_GetDroppedCount(void) {
    return tx_dropped;
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
    char buf[10];
    uint8_t i = 0U;
    uint32_t v = value;

    if (v == 0U) {
        UART2_SendChar('0');
    } else {
        while (v > 0U) {
            buf[i] = (char)((v % 10U) + (uint32_t)'0');
            i++;
            v /= 10U;
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
    }

    integer_part = (int32_t)v;
    decimal_part = (int32_t)(((v - (float)integer_part) * 10.0f) + 0.5f);

    if (decimal_part >= 10) {
        decimal_part = 0;
        integer_part++;
    }

    UART2_SendUint((uint32_t)integer_part);
    UART2_SendChar('.');
    UART2_SendUint((uint32_t)decimal_part);
}
