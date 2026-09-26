#ifndef SAFETY_H_
#define SAFETY_H_

#include "stm32f411xx_custom.h"

/* เงื่อนไขความปลอดภัย (ฉบับปรับปรุง): ตรวจทั้งอุณหภูมิ MCU (ADC1) และความชื้น (DHT11) */
#define SAFETY_TEMP_MAX_C      (30.0f)
#define SAFETY_HUMID_MAX_PCT   (70U)

/* Digital Output แยกต่างหาก (นอกเหนือจาก LED2 บนบอร์ด): HIGH เมื่ออุณหภูมิเกินเกณฑ์เท่านั้น
 * (ไม่ผูกกับความชื้น) ใช้ต่อออกไปขับอุปกรณ์ภายนอกเพิ่มเติมได้ เช่น Buzzer/Relay/Fan ผ่าน Transistor
 */
#define TEMP_ALARM_OUT_PORT    GPIOC
#define TEMP_ALARM_OUT_PIN     2U   /* PC2 */

/* เรียกครั้งเดียวตอนเริ่มระบบ (หลัง GPIO_Init) เพื่อตั้งค่า LED1-3 และขา PC2 เริ่มต้น */
void Safety_Init(void);

/* Background Task: เรียกทุกครั้งที่มีค่า Sensor ใหม่เข้ามา (Temp จาก ADC1, Humid จาก DHT11)
 * ทำหน้าที่ทั้งหมดของการตรวจสอบสภาวะแวดล้อมในที่เดียว:
 *   1. ควบคุม LED1 (Normal) / LED2 (Temp Alarm) / LED3 (Humid Alarm) และขา PC2 (Temp Alarm Output)
 *      (ไม่มีการพิมพ์สถานะออก UART ตอนปกติ - เงียบสนิท จนกว่าจะเกินเกณฑ์ ดูข้อ 2)
 *   2. พิมพ์ข้อความ [WARNING] วนซ้ำทุกครั้งที่ Update ตราบใดที่ยังเกินเกณฑ์อยู่ (Temp และ/หรือ Humid)
 *   3. ติดตามสถานะ Lockout และพิมพ์ข้อความแจ้งเพียงครั้งเดียวตอนเปลี่ยนสถานะ Lock/Unlock (Auto-Recovery)
 */
void Safety_Update(float temp_c, uint8_t humidity_pct);

/* คืนค่า 1 = ระบบถูกล็อก ห้ามทำรายการซื้อขายใด ๆ, 0 = ปกติ ทำรายการได้ตามปกติ */
uint8_t Safety_IsLockout(void);

#endif /* SAFETY_H_ */
