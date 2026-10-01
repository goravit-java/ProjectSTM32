#ifndef I2C_DRIVER_H_
#define I2C_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* Software I2C (Bit-bang) บนขา GPIO ธรรมดา
 * เหตุผล: PC8/PC6 ไม่มีฟังก์ชัน I2C ฮาร์ดแวร์บน STM32F411 (I2C1 ใช้ได้แค่ PB6-PB9, I2C3 ใช้ PA8/PC9)
 * จึงต้องสร้างสัญญาณ I2C เองด้วยการสลับขา GPIO แบบ Open-Drain ตามจังหวะของโปรโตคอล
 */
#define SI2C_PORT       GPIOC
#define SI2C_SCL_PIN    8U   /* PC8 = SCL */
#define SI2C_SDA_PIN    6U   /* PC6 = SDA */

/* ตั้งค่าขา SCL/SDA เป็น Open-Drain Output + Pull-up แล้วกู้ Bus เผื่อ Slave ค้างจากการ Reset รอบก่อน */
void SoftI2C_Init(void);

/* ส่งข้อมูลหลาย Byte ไปยังอุปกรณ์ I2C (Address 7-bit) แบบ Blocking
 * คืนค่า 1 = สำเร็จ, 0 = ล้มเหลว (Slave ไม่ตอบ ACK เช่น ไม่มีอุปกรณ์ต่ออยู่จริง/สายหลุด/Address ผิด)
 * ไม่มี Loop รอแบบไม่สิ้นสุด จึงปลอดภัยกับ IWDG แม้จอไม่ได้ต่ออยู่
 */
uint8_t SoftI2C_WriteBytes(uint8_t dev_addr7, const uint8_t *data, uint16_t len);

#endif /* I2C_DRIVER_H_ */
