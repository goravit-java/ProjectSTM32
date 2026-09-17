#include "uart_driver.h"

void UART2_Init(void) {
    /* 1. เปิด Clock ให้ GPIOA และ USART2 */
    RCC->AHB1ENR |= (1U << 0);          // GPIOA Clock Enable
    RCC->APB1ENR |= (1U << 17);         // USART2 Clock Enable

    /* 2. ตั้งค่า PA2 (TX) และ PA3 (RX) เป็น Alternate Function 7 (AF07) */
    GPIOA->MODER &= ~((3U << (2 * 2)) | (3U << (3 * 2)));
    GPIOA->MODER |=  ((2U << (2 * 2)) | (2U << (3 * 2))); // Alternate Function mode

    GPIOA->AFR[0] &= ~((0xFU << (2 * 4)) | (0xFU << (3 * 4)));
    GPIOA->AFR[0] |=  ((0x7U << (2 * 4)) | (0x7U << (3 * 4))); // AF07 (USART2)

    /* 3. ตั้งค่า Baud Rate = 115200 (คำนวณจาก Clock HSI 16 MHz) */
    USART2->BRR = 0x008A;

    /* 4. เปิดใช้งาน Transmitter, Receiver และ USART2 */
    USART2->CR1 |= (1U << 3) | (1U << 2) | (1U << 13); // TE, RE, UE
}

void UART2_SendChar(char c) {
    while (!(USART2->SR & (1U << 7))); // รอจน TXE = 1 (พร้อมส่ง)
    USART2->DR = (c & 0xFF);
}

void UART2_SendString(char *str) {
    while (*str) {
        UART2_SendChar(*str++);
    }
}
