#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* USART2 (PA2 = TX, PA3 = RX) ต่อผ่าน ST-LINK Virtual COM Port, 115200 8N1
 * การส่งข้อมูลทำงานแบบ Interrupt-driven: ฟังก์ชัน Send* เพียงนำข้อมูลใส่ Ring Buffer แล้วคืนค่าทันที
 * จากนั้น USART2_IRQHandler (TXE Interrupt) จะทยอยส่งทีละ Byte ออกไปเองเบื้องหลัง (ไม่มี Polling)
 */
#define UART_TX_BUFFER_SIZE   1024U   /* ต้องเป็นเลขยกกำลัง 2 เพื่อใช้ & แทน % ในการวนกลับ Index */

void UART2_Init(void);
void UART2_SendChar(char c);
void UART2_SendString(const char *str);
void UART2_SendUint(uint32_t value);
void UART2_SendFloat1(float value);

/* จำนวนตัวอักษรที่ถูกทิ้งเพราะ Buffer เต็ม (ไว้ตรวจสอบตอน Debug ว่า Buffer เล็กเกินไปหรือไม่) */
uint32_t UART2_GetDroppedCount(void);

#endif /* UART_DRIVER_H_ */
