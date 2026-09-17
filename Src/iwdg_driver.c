#include "iwdg_driver.h"

void IWDG_Init(void) {
    IWDG->KR = 0x5555; // ปลดล็อกการเขียน Register
    IWDG->PR = 0x04;   // Prescaler /64 (นับช้าลงเพื่อให้มีเวลาเลี้ยงหมา)
    IWDG->RLR = 0x0FFF; // Reload Value สูงสุด (~4-5 วินาที)
    IWDG->KR = 0xAAAA; // โหลดค่าเริ่มต้น
    IWDG->KR = 0xCCCC; // เปิดใช้งาน IWDG
}

void IWDG_Refresh(void) {
    IWDG->KR = 0xAAAA; // เลี้ยงหมา (Reset Counter) เพื่อไม่ให้ MCU รีเซ็ตตัวเอง
}
