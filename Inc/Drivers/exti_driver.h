#ifndef EXTI_DRIVER_H_
#define EXTI_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* External Interrupt (EXTI) สำหรับขา Input ที่เป็น Active-Low (เช่น ปุ่มกดต่อลง GND)
 * - ตรวจจับขอบขาลง (Falling Edge = เริ่มกด) ด้วย Hardware แล้ว ISR บันทึก Event + เวลาไว้
 * - กันการเด้งของหน้าสัมผัส (Debounce) 2 ชั้น:
 *     1) ISR ไม่รับ Edge ใหม่ภายใน EXTI_GUARD_US หลัง Edge ที่รับไปแล้วของ Line เดียวกัน
 *     2) EXTI_TakeEvent() จะส่ง Event ออกไปก็ต่อเมื่อผ่านไปแล้วอย่างน้อย EXTI_SETTLE_US
 *        เพื่อให้ผู้เรียกอ่านระดับขาตอนที่หน้าสัมผัสนิ่งแล้ว
 * หมายเหตุ: ต้องเรียก TIM2_Init() ก่อน (ใช้เป็นฐานเวลาระดับ us)
 */
#define EXTI_GUARD_US    100000U  /* 100 ms */
#define EXTI_SETTLE_US   10000U   /* 10 ms */

/* ผูกขา pin ของ port (GPIOA/GPIOB/GPIOC) เข้ากับ EXTI Line เดียวกับเลขขา แล้วเปิด Interrupt
 * ขาต้องถูกตั้งเป็น Input (+Pull-up) ไว้ก่อนแล้ว
 */
void EXTI_InitFallingEdge(GPIO_TypeDef *port, uint8_t pin);

/* คืนค่า 1 ถ้ามี Event ที่นิ่งแล้วรออยู่บน Line นี้ (และเคลียร์ Event ทิ้ง), 0 ถ้าไม่มี */
uint8_t EXTI_TakeEvent(uint8_t pin);

#endif /* EXTI_DRIVER_H_ */
