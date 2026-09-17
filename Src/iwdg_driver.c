#include "iwdg_driver.h"

/* IWDG ทำงานด้วย LSI Clock ภายใน (~32 kHz) และแยกอิสระจาก Clock หลักของระบบ
 * Timeout = (Prescaler * (RLR + 1)) / LSI_Freq
 * เลือก Prescaler = /32, RLR = 999 -> Timeout ~= (32 * 1000) / 32000 = 1.0 วินาที
 * ดังนั้นในโปรแกรมหลักต้องเรียก IWDG_Refresh() อย่างน้อยทุก 1 วินาที ไม่เช่นนั้น MCU จะ Reset ตัวเอง
 */
void IWDG_Init(void) {
    IWDG->KR = 0x5555U;                 // ปลดล็อกการเขียน PR และ RLR
    while ((IWDG->SR & (1U << 0)) != 0U) { } /* รอจนบิต PVU (Prescaler Value Update) ว่าง */

    IWDG->PR = 0x03U;                   // Prescaler = /32

    while ((IWDG->SR & (1U << 1)) != 0U) { } /* รอจนบิต RVU (Reload Value Update) ว่าง */
    IWDG->RLR = 999U;                   // Reload value -> Timeout ~ 1 วินาที

    IWDG->KR = 0xAAAAU;                 // Refresh ครั้งแรก เพื่อโหลดค่า RLR เข้า Down-counter
    IWDG->KR = 0xCCCCU;                 // เริ่มการทำงานของ Watchdog (เริ่มนับถอยหลัง)
}

void IWDG_Refresh(void) {
    IWDG->KR = 0xAAAAU;                 // "เลี้ยง" Watchdog (Kick the dog) ป้องกันการ Reset
}
