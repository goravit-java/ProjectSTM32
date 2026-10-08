#ifndef LIGHT_SENSOR_DRIVER_H_
#define LIGHT_SENSOR_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* โมดูลเซ็นเซอร์แสง LDR (ชิปเปรียบเทียบ LM393) ใช้ขา DO แบบดิจิทัล ต่อเข้า PA1 (Arduino A1)
 * ใช้เป็น "ช่องหยอดเหรียญ": บังแสง 1 ครั้ง = รับเงิน 1 ครั้ง
 */
#define LIGHT_SENSOR_PORT       GPIOA
#define LIGHT_SENSOR_PIN        1U    /* PA1 -> EXTI1 */

/* ระดับของขา DO ตอน "ถูกบังแสง"
 * โมดูลทั่วไป: สว่าง = DO เป็น LOW (ไฟ D0-LED บนโมดูลติด), มืด/ถูกบัง = DO เป็น HIGH
 * ถ้าโมดูลของคุณทำงานกลับด้าน (นับเงินตอนเอามือออกแทนตอนบัง) ให้เปลี่ยนค่านี้เป็น 0U
 */
#define LIGHT_BLOCKED_LEVEL     1U

/* ตั้งขา PA1 เป็น Input แล้วเปิด EXTI ที่ขอบ "เริ่มถูกบัง" (ต้องเรียกหลัง TIM2_Init และ GPIO_Init) */
void LightSensor_Init(void);

/* คืนค่า 1 ครั้งเดียวต่อการบัง 1 ครั้ง: มี Interrupt ขอบเริ่มบัง และหลังสัญญาณนิ่งแล้วยังบังอยู่จริง
 * การบังค้างไว้นาน ๆ จะไม่ถูกนับซ้ำ เพราะ Interrupt เกิดเฉพาะตอนขอบสัญญาณเปลี่ยนเท่านั้น
 */
uint8_t LightSensor_TakeBlockEvent(void);

#endif /* LIGHT_SENSOR_DRIVER_H_ */
