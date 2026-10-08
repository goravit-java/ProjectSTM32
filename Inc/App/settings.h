#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdint.h>

/* หน้า SETTINGS: ตั้งเกณฑ์อุณหภูมิและความชื้นด้วย Potentiometer (PA4)
 * วิธีใช้: ที่หน้าแรกกด BACK ค้าง 1.5 วินาที -> UP/DOWN เลือกหัวข้อ -> หมุนปุ่มปรับค่า -> OK บันทึก / BACK ยกเลิก
 * - ค่าที่กำลังตั้งเริ่มจากเกณฑ์ปัจจุบันเสมอ และจะเริ่มเปลี่ยนตามปุ่มหมุนก็ต่อเมื่อหมุนจริง (กันค่ากระโดดเอง
 *   เพราะตำแหน่งปุ่มหมุนอาจไม่ตรงกับค่าเดิม) เปลี่ยนหัวข้อแล้วต้องหมุนใหม่ถึงจะเริ่มตาม
 * - นอกหน้า SETTINGS ปุ่มหมุนไม่มีผลกับระบบ (กันการไปโดนปุ่มหมุนแล้วเกณฑ์เปลี่ยนโดยไม่มีใครรู้)
 */
#define SETTINGS_FIELD_TEMP    0U
#define SETTINGS_FIELD_HUMID   1U

void Settings_Enter(void);                  /* เริ่มแก้ไข: ค่าที่กำลังตั้ง = เกณฑ์ปัจจุบัน */
void Settings_Exit(void);                   /* เลิกแก้ไข (ไม่บันทึก) */
void Settings_Save(void);                   /* บันทึกค่าที่กำลังตั้งลง safety.c แล้วเลิกแก้ไข */
void Settings_NextField(void);              /* สลับหัวข้อ อุณหภูมิ <-> ความชื้น */
void Settings_OnKnob(uint16_t knob_raw);    /* เรียกทุกครั้งที่มีค่าปุ่มหมุนใหม่ (ทุก 100 ms) */
uint8_t Settings_TakeActivity(void);        /* 1 = ค่าเปลี่ยนจากการหมุนตั้งแต่เรียกครั้งก่อน (ใช้รีเซ็ต Timeout) */

uint8_t Settings_GetField(void);            /* หัวข้อที่เลือกอยู่ (SETTINGS_FIELD_*) */
uint8_t Settings_GetPendingTempStep(void);  /* ค่าอุณหภูมิที่กำลังตั้ง (ขั้น 0-60) */
uint8_t Settings_GetPendingHumid(void);     /* ค่าความชื้นที่กำลังตั้ง (%) */

#endif /* SETTINGS_H_ */
