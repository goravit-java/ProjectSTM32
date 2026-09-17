#ifndef FSM_H_
#define FSM_H_

#include "stm32f411xx_custom.h"

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

#endif /* FSM_H_ */
