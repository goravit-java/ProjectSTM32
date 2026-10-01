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
    STATE_SAFETY_CHECK,
    STATE_PROCESSING,
    STATE_COMPLETE,
    STATE_FAULT
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
uint32_t FSM_GetSecondsLeft(void);      /* วินาทีที่เหลือระหว่าง PROCESSING (คืนค่า 0 ถ้าไม่ได้อยู่ใน State นี้) */

#endif /* FSM_H_ */
