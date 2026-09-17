#include "stm32f411xx_custom.h"
#include "gpio_driver.h"
#include "uart_driver.h"

static void delay_ms(volatile uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 3000; i++) {
        __asm("NOP");
    }
}
/* ฟังก์ชันสำหรับส่งค่าอุณหภูมิ float (ทศนิยม 2 ตำแหน่ง) ออกทาง UART */
void UART2_PrintTemperature(float temp) {
    if (temp < 0.0f) {
        UART2_SendString("TIMEOUT ERROR!\r\n");
        return;
    }

    int integer_part = (int)temp;
    int decimal_part = (int)((temp - (float)integer_part) * 100.0f);

    UART2_SendString("Current Temp: ");

    /* แปลงและส่งเลขส่วนจำนวนเต็ม */
    char buf[10];
    int i = 0;
    if (integer_part == 0) UART2_SendChar('0');
    while (integer_part > 0) {
        buf[i++] = (integer_part % 10) + '0';
        integer_part /= 10;
    }
    while (i > 0) UART2_SendChar(buf[--i]);

    UART2_SendString(".");

    /* เติม 0 นำหน้ากรณีทศนิยมหลักเดียว เช่น .05 */
    if (decimal_part < 10) UART2_SendString("0");

    /* แปลงและส่งเลขส่วนทศนิยม */
    i = 0;
    if (decimal_part == 0) UART2_SendChar('0');
    while (decimal_part > 0) {
        buf[i++] = (decimal_part % 10) + '0';
        decimal_part /= 10;
    }
    while (i > 0) UART2_SendChar(buf[--i]);

    UART2_SendString(" C\r\n");
}
int main(void) {
    GPIO_Init();
    UART2_Init();

    UART2_SendString("\r\n========================================\r\n");
    UART2_SendString("   Smart Vending Machine Controller     \r\n");
    UART2_SendString("   Day 1: Hardware & Comms Initialized  \r\n");
    UART2_SendString("========================================\r\n");

    while (1) {
        /* ใช้ else if เพื่อให้ประมวลผลได้ทีละ 1 ปุ่มเท่านั้น ป้องกันการกดพร้อมกัน */
        if (BTN_IsPressed(BTN_UP_PORT, BTN_UP_PIN)) {
            LED_On(LED1_PORT, LED1_PIN);
            UART2_SendString("[INPUT] Button UP Pressed\r\n");
            delay_ms(250); // กันกดซ้ำ/กดค้าง
        }
        else if (BTN_IsPressed(BTN_DOWN_PORT, BTN_DOWN_PIN)) {
            LED_On(LED2_PORT, LED2_PIN);
            UART2_SendString("[INPUT] Button DOWN Pressed\r\n");
            delay_ms(250);
        }
        else if (BTN_IsPressed(BTN_OK_PORT, BTN_OK_PIN)) {
            LED_On(LED3_PORT, LED3_PIN);
            UART2_SendString("[INPUT] Button OK Pressed\r\n");
            delay_ms(250);
        }
        else if (BTN_IsPressed(BTN_BACK_PORT, BTN_BACK_PIN)) {
            LED_On(LED4_PORT, LED4_PIN);
            UART2_SendString("[INPUT] Button BACK Pressed\r\n");
            delay_ms(250);
            UART2_PrintTemperature(10);
        }
        else {
            /* เมื่อไม่มีการกดปุ่มใดๆ ให้ปิด LED ทั้งหมด */
            LED_Off(LED1_PORT, LED1_PIN);
            LED_Off(LED2_PORT, LED2_PIN);
            LED_Off(LED3_PORT, LED3_PIN);
            LED_Off(LED4_PORT, LED4_PIN);
        }
    }
}
