#ifndef EXTI_DRIVER_H_
#define EXTI_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* External Interrupt (EXTI) สำหรับขา Input ดิจิทัล (ปุ่มกด Active-Low, เซ็นเซอร์แสงรับเงิน)
 * - ตรวจจับขอบสัญญาณที่เลือก (ขาลงหรือขาขึ้น) ด้วย Hardware แล้ว ISR บันทึก Event + เวลาไว้
 * - กันการเด้งของหน้าสัมผัส (Debounce) 2 ชั้น:
 *     1) ISR ไม่รับ Edge ใหม่ภายใน EXTI_GUARD_US หลัง Edge ที่รับไปแล้วของ Line เดียวกัน
 *     2) EXTI_TakeEvent() จะส่ง Event ออกไปก็ต่อเมื่อผ่านไปแล้วอย่างน้อย EXTI_SETTLE_US
 *        เพื่อให้ผู้เรียกอ่านระดับขาตอนที่หน้าสัมผัสนิ่งแล้ว
 * หมายเหตุ: ต้องเรียก TIM2_Init() ก่อน (ใช้เป็นฐานเวลาระดับ us)
 */
#define EXTI_GUARD_US    100000U  /* 100 ms */
#define EXTI_SETTLE_US   10000U   /* 10 ms */

/* ขอบสัญญาณที่ต้องการให้เกิด Interrupt */
#define EXTI_EDGE_FALLING   0U   /* HIGH -> LOW (เช่น กดปุ่ม Active-Low) */
#define EXTI_EDGE_RISING    1U   /* LOW -> HIGH (เช่น เซ็นเซอร์แสงถูกบัง) */
#define EXTI_EDGE_BOTH      2U   /* ทั้งสองขอบ (เช่น ปุ่มที่ต้องรู้ทั้งตอนกดและตอนปล่อย เพื่อจับการกดค้าง) */

/* ผูกขา pin ของ port (GPIOA/GPIOB/GPIOC) เข้ากับ EXTI Line เดียวกับเลขขา แล้วเปิด Interrupt ที่ขอบ edge
 * ขาต้องถูกตั้งเป็น Input ไว้ก่อนแล้ว
 */
void EXTI_InitEdge(GPIO_TypeDef *port, uint8_t pin, uint8_t edge);

/* คืนค่า 1 ถ้ามี Event ที่นิ่งแล้วรออยู่บน Line นี้ (และเคลียร์ Event ทิ้ง), 0 ถ้าไม่มี */
uint8_t EXTI_TakeEvent(uint8_t pin);

#endif /* EXTI_DRIVER_H_ */
