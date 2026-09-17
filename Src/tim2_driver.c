#include "tim2_driver.h"

/* ตั้งค่า TIM2 ให้เป็น Free-running Counter ที่นับ 1 Tick = 1 microsecond
 * ใช้แทนการหน่วงเวลาแบบ NOP loop เดิม เพราะ DHT11 ต้องการความแม่นยำระดับ us
 * สมมติ Core Clock = HSI 16MHz, APB1 Prescaler = /1 (ค่า Default ของระบบ ไม่เคยถูกแก้ไข)
 * ดังนั้น TIM2CLK = 16MHz -> ตั้ง Prescaler = 16 เพื่อให้ได้ Counter Clock = 1MHz (1 Tick = 1us)
 */
void TIM2_Init(void) {
    RCC->APB1ENR |= (1U << 0); // TIM2EN

    TIM2->PSC = 16U - 1U;
    TIM2->ARR = 0xFFFFFFFFU;   // Free-running นับสูงสุด (TIM2 เป็น Counter 32-bit)
    TIM2->EGR |= (1U << 0);    // UG: บังคับ Update ทันที ให้ค่า Prescaler มีผลจริง
    TIM2->CR1 |= (1U << 0);    // CEN: เริ่มนับ
}

uint32_t TIM2_GetMicros(void) {
    return TIM2->CNT;
}

/* หน่วงเวลาแบบแม่นยำระดับ microsecond โดยใช้ค่า Counter จริงของ TIM2
 * การลบแบบ Unsigned ทำให้ถูกต้องแม้ Counter จะ Overflow วนกลับมาระหว่างรอ
 */
void TIM2_DelayUs(uint32_t us) {
    uint32_t start = TIM2_GetMicros();
    while ((TIM2_GetMicros() - start) < us) { }
}
