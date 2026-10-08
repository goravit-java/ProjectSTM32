#ifndef FSM_H_
#define FSM_H_

#include <stdint.h>

/* State หลักของระบบ ตามที่ระบุไว้ในเอกสารภาพรวมโครงงาน */
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_SELECT_DRINK,
    STATE_CONFIRM,
    STATE_CHECK_STOCK,
    STATE_PAYMENT,          /* รับชำระเงินผ่านเซ็นเซอร์แสง (บัง 1 ครั้ง = 10 บาท) + นับถอยหลัง 30 วินาที */
    STATE_PAYMENT_FAILED,   /* หมดเวลาแต่ยอดเงินไม่ครบ: แจ้งเตือน 3 วินาทีแล้วกลับ IDLE */
    STATE_SAFETY_CHECK,
    STATE_PROCESSING,
    STATE_COMPLETE,
    STATE_FAULT             /* LOCKOUT_ALARM: ล็อกระบบเมื่อ Temp/Humid เกินเกณฑ์ */
} SystemState_t;

/* เรียกครั้งเดียวตอนเริ่มโปรแกรม */
void FSM_Init(void);

/* เรียกทุกรอบของ Main Loop (ไม่ Block) — อ่านปุ่ม + ประมวลผล 1 Tick ของ State ปัจจุบัน */
void FSM_Run(void);

/* คืนค่า State ปัจจุบัน (เผื่อ main.c หรือโมดูลอื่นอยากรู้) */
SystemState_t FSM_GetState(void);

/* Getter เพิ่มเติมสำหรับ display.c (จอ OLED) อ่านไปวาดหน้าจอ โดยไม่ต้องรู้ Internal State ของ FSM */
uint8_t FSM_GetSelectedIndex(void);     /* Index สินค้าที่เลือกอยู่ปัจจุบัน (0 ถึง MENU_ITEM_COUNT-1) */
uint32_t FSM_GetProgressPercent(void);  /* ความคืบหน้า 0-100% ระหว่าง PROCESSING (คืนค่า 100 ตอน COMPLETE, 0 นอกจากนั้น) */
uint32_t FSM_GetSecondsLeft(void);      /* วินาทีที่เหลือระหว่าง PROCESSING หรือ PAYMENT (คืนค่า 0 ถ้าไม่ได้อยู่ใน 2 State นี้) */
uint32_t FSM_GetPaidAmount(void);       /* ยอดเงินที่ลูกค้าชำระแล้วในรายการปัจจุบัน (บาท) */
uint8_t FSM_IsOrderCancelled(void);     /* 1 = รายการล่าสุดถูกยกเลิกเพราะระบบล็อกกลางคัน (ล้างเป็น 0 เมื่อกลับ IDLE) */

#endif /* FSM_H_ */
