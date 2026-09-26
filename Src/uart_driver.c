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
    while (!(USART2->SR & (1U << 7))) { } /* รอจน TXE = 1 (พร้อมส่ง) */
    USART2->DR = ((uint32_t)c & 0xFFU);
}

void UART2_SendString(char *str) {
    while (*str != '\0') {
        UART2_SendChar(*str);
        str++;
    }
}

/* ส่งค่าจำนวนเต็มไม่ติดลบ (เช่น ราคา, Stock) ออกทาง UART โดยไม่ใช้ sprintf/printf
 * (หลีกเลี่ยงการพึ่งพา Standard I/O Library ซึ่งกินพื้นที่ Flash มากบนระบบ Bare-metal)
 */
void UART2_SendUint(uint32_t value) {
    char buf[10];
    uint8_t i = 0U;

    if (value == 0U) {
        UART2_SendChar('0');
        return;
    }

    while (value > 0U) {
        buf[i] = (char)((value % 10U) + '0');
        i++;
        value /= 10U;
    }

    while (i > 0U) {
        i--;
        UART2_SendChar(buf[i]);
    }
}

/* ส่งค่าอุณหภูมิ float (ทศนิยม 2 ตำแหน่ง) ออกทาง UART
 * (ย้ายมาจาก main.c เดิมของ Day 1 และปรับให้ใช้ UART2_SendUint ร่วมกัน ลดโค้ดซ้ำซ้อน)
 */
void UART2_PrintTemperature(float temp) {
    int32_t integer_part;
    int32_t decimal_part;

    if (temp < 0.0f) {
        UART2_SendString("TIMEOUT ERROR!\r\n");
        return;
    }

    integer_part = (int32_t)temp;
    decimal_part = (int32_t)((temp - (float)integer_part) * 100.0f);

    UART2_SendString("Current Temp: ");
    UART2_SendUint((uint32_t)integer_part);
    UART2_SendString(".");

    if (decimal_part < 10) {
        UART2_SendString("0"); /* เติม 0 นำหน้ากรณีทศนิยมหลักเดียว เช่น .05 */
    }
    UART2_SendUint((uint32_t)decimal_part);
    UART2_SendString(" C\r\n");
}

/* ส่งค่า float แบบทศนิยม 1 ตำแหน่ง โดยไม่มี Label/หน่วยต่อท้าย (เช่น "43.5")
 * ใช้เป็น Building Block กลางสำหรับประกอบข้อความแจ้งเตือนต่าง ๆ ใน safety.c
 * ปัดเศษทศนิยมตำแหน่งที่ 1 อย่างถูกต้อง (รวมกรณีทด เช่น 29.96 -> "30.0" ไม่ใช่ "29.10")
 */
void UART2_SendFloat1(float value) {
    int32_t integer_part;
    int32_t decimal_part;

    if (value < 0.0f) {
        UART2_SendChar('-');
        value = -value;
    }

    integer_part = (int32_t)value;
    decimal_part = (int32_t)(((value - (float)integer_part) * 10.0f) + 0.5f);

    if (decimal_part >= 10) {
        decimal_part = 0;
        integer_part++;
    }

    UART2_SendUint((uint32_t)integer_part);
    UART2_SendString(".");
    UART2_SendUint((uint32_t)decimal_part);
}
