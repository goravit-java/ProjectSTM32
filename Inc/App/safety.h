#ifndef SAFETY_H_
#define SAFETY_H_

#include "Drivers/stm32f411xx_custom.h"

/* เงื่อนไขความปลอดภัย (ฉบับปรับปรุง): ตรวจทั้งอุณหภูมิ (NTC ที่ PA0 ผ่าน ADC1) และความชื้น (DHT11) */
#define SAFETY_TEMP_MAX_C      (40.0f)
#define SAFETY_HUMID_MAX_PCT   (75U)

/* Digital Output ขับ Relay พัดลมระบายอากาศ: HIGH เมื่ออุณหภูมิหรือความชื้นเกินเกณฑ์ (ระบบ Lockout)
 * LOW เมื่อสภาวะแวดล้อมกลับมาปกติ
 */
#define TEMP_ALARM_OUT_PORT    GPIOC
#define TEMP_ALARM_OUT_PIN     2U   /* PC2 */

/* เรียกครั้งเดียวตอนเริ่มระบบ (หลัง GPIO_Init) เพื่อตั้งค่า LED1-3 และขา PC2 เริ่มต้น */
void Safety_Init(void);

/* Background Task: เรียกทุกครั้งที่มีค่า Sensor ใหม่เข้ามา (Temp จาก NTC/ADC1, Humid จาก DHT11)
 * ทำหน้าที่ทั้งหมดของการตรวจสอบสภาวะแวดล้อมในที่เดียว:
 *   1. ควบคุม LED1 (Normal) / LED2 (Temp Alarm) / LED3 (Humid Alarm) และขา PC2 (Relay พัดลม: Temp หรือ Humid เกิน)
 *      (ไม่มีการพิมพ์สถานะออก UART ตอนปกติ - เงียบสนิท จนกว่าจะเกินเกณฑ์ ดูข้อ 2)
 *   2. พิมพ์ข้อความ [WARNING] วนซ้ำทุกครั้งที่ Update ตราบใดที่ยังเกินเกณฑ์อยู่ (Temp และ/หรือ Humid)
 *   3. ติดตามสถานะ Lockout และพิมพ์ข้อความแจ้งเพียงครั้งเดียวตอนเปลี่ยนสถานะ Lock/Unlock (Auto-Recovery)
 */
void Safety_Update(float temp_c, uint8_t humidity_pct);

/* คืนค่า 1 = ระบบถูกล็อก ห้ามทำรายการซื้อขายใด ๆ, 0 = ปกติ ทำรายการได้ตามปกติ */
uint8_t Safety_IsLockout(void);

/* Getter ค่า Sensor ล่าสุดที่เคยผ่านเข้ามาทาง Safety_Update() สำหรับโมดูลอื่น (เช่น display.c)
 * อ่านไปแสดงผลเฉย ๆ ไม่ใช้ตัดสินใจ Logic ซ้ำที่อื่น (safety.c เป็นเจ้าของการตัดสินใจแต่เพียงผู้เดียว)
 */
float Safety_GetLastTemp(void);
uint8_t Safety_HasReading(void);   /* 1 = มีค่า Sensor จริงเข้ามาแล้วอย่างน้อย 1 ครั้ง (ก่อนหน้านั้น Getter คืนค่า 0) */
uint8_t Safety_GetLastHumidity(void);

#endif /* SAFETY_H_ */
