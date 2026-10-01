#include "Drivers/iwdg_driver.h"

#define IWDG_KEY_UNLOCK   0x5555U  /* ปลดล็อกการเขียน PR และ RLR */
#define IWDG_KEY_RELOAD   0xAAAAU  /* โหลดค่า RLR เข้า Down-counter ("เลี้ยง" Watchdog) */
#define IWDG_KEY_START    0xCCCCU  /* เริ่มการทำงานของ Watchdog */

#define IWDG_SR_PVU       (1U << 0U)
#define IWDG_SR_RVU       (1U << 1U)

/* IWDG ทำงานด้วย LSI Clock ภายใน (~32 kHz) และแยกอิสระจาก Clock หลักของระบบ
 * Timeout = (Prescaler * (RLR + 1)) / LSI_Freq
 * เลือก Prescaler = /32, RLR = 999 -> Timeout ~= (32 * 1000) / 32000 = 1.0 วินาที
 * ดังนั้นในโปรแกรมหลักต้องเรียก IWDG_Refresh() อย่างน้อยทุก 1 วินาที ไม่เช่นนั้น MCU จะ Reset ตัวเอง
 */
void IWDG_Init(void) {
    IWDG->KR = IWDG_KEY_UNLOCK;

    while ((IWDG->SR & IWDG_SR_PVU) != 0U) {
        /* รอจน Prescaler Value Update ว่าง (ใช้เวลาไม่กี่รอบ LSI ทำครั้งเดียวตอนเริ่มระบบ) */
    }
    IWDG->PR = 0x03U;                   /* Prescaler = /32 */

    while ((IWDG->SR & IWDG_SR_RVU) != 0U) {
        /* รอจน Reload Value Update ว่าง */
    }
    IWDG->RLR = 999U;                   /* Reload value -> Timeout ~ 1 วินาที */

    IWDG->KR = IWDG_KEY_RELOAD;
    IWDG->KR = IWDG_KEY_START;
}

void IWDG_Refresh(void) {
    IWDG->KR = IWDG_KEY_RELOAD;
}
