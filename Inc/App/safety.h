#ifndef SAFETY_H_
#define SAFETY_H_

#include "Drivers/stm32f411xx_custom.h"

/* เงื่อนไขความปลอดภัย (ฉบับปรับปรุง): ตรวจทั้งอุณหภูมิ (NTC ที่ PA0 ผ่าน ADC1) และความชื้น (DHT11)
 * เกณฑ์ทั้งสองตั้งได้จากหน้า SETTINGS (ดู SAFETY_TEMP_LIMIT_* / SAFETY_HUMID_LIMIT_*) */
/* เกณฑ์อุณหภูมิและความชื้นตั้งได้จากหน้า SETTINGS (กด BACK ค้างที่หน้าแรก แล้วหมุน Potentiometer PA4)
 * อุณหภูมิ: 20.0 ถึง 50.0 °C ทีละ 0.5 °C (เก็บเป็น "ขั้น" 0-60 เพื่อไม่ต้องเทียบ float ด้วย ==)
 * ความชื้น: 40 ถึง 90 % ทีละ 1 %
 * ค่าเริ่มต้นหลังเปิดเครื่อง 40.0 °C และ 70 % (เก็บใน RAM ปิดเครื่องแล้วกลับเป็นค่าเริ่มต้น)
 */
#define SAFETY_TEMP_LIMIT_MIN_C      (20.0f)
#define SAFETY_TEMP_LIMIT_STEP_C     (0.5f)
#define SAFETY_TEMP_LIMIT_STEPS      60U    /* (50.0 - 20.0) / 0.5 */
#define SAFETY_TEMP_LIMIT_DEFAULT    40U    /* ขั้นที่ 40 = 20.0 + (40 x 0.5) = 40.0 °C */
#define SAFETY_HUMID_LIMIT_MIN_PCT   40U
#define SAFETY_HUMID_LIMIT_MAX_PCT   90U
#define SAFETY_HUMID_LIMIT_DEFAULT   70U

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

/* บันทึกเกณฑ์ใหม่ (เรียกจากหน้า SETTINGS ตอนกด OK) ค่าที่เกินช่วงจะถูกตัดให้อยู่ในช่วง
 * พิมพ์ "[SET] ..." ทาง UART และตรวจ Lockout ใหม่ทันทีด้วยค่า Sensor ล่าสุด (ไม่ต้องรอรอบ 2 วินาที)
 */
void Safety_SetLimits(uint8_t temp_step, uint8_t humid_pct);
float Safety_GetTempLimit(void);          /* เกณฑ์อุณหภูมิปัจจุบัน (°C) */
uint8_t Safety_GetTempLimitStep(void);    /* เกณฑ์อุณหภูมิปัจจุบันเป็นขั้น (0-60) */
uint8_t Safety_GetHumidLimit(void);       /* เกณฑ์ความชื้นปัจจุบัน (%) */
float Safety_TempStepToC(uint8_t step);   /* แปลงขั้น -> °C (ใช้แสดงค่าที่กำลังตั้งบนจอ) */

/* คืนค่า 1 = ระบบถูกล็อก ห้ามทำรายการซื้อขายใด ๆ, 0 = ปกติ ทำรายการได้ตามปกติ */
uint8_t Safety_IsLockout(void);

/* Getter ค่า Sensor ล่าสุดที่เคยผ่านเข้ามาทาง Safety_Update() สำหรับโมดูลอื่น (เช่น display.c)
 * อ่านไปแสดงผลเฉย ๆ ไม่ใช้ตัดสินใจ Logic ซ้ำที่อื่น (safety.c เป็นเจ้าของการตัดสินใจแต่เพียงผู้เดียว)
 */
float Safety_GetLastTemp(void);
uint8_t Safety_HasReading(void);   /* 1 = มีค่า Sensor จริงเข้ามาแล้วอย่างน้อย 1 ครั้ง (ก่อนหน้านั้น Getter คืนค่า 0) */
uint8_t Safety_GetLastHumidity(void);

#endif /* SAFETY_H_ */
