#include "Drivers/tim2_driver.h"

#define TIM2_CLOCK_MHZ      16U           /* TIMCLK = HSI 16 MHz (APB1 Prescaler = /1 ค่า Default) */
#define TIM2_ARR_MAX        0xFFFFFFFFU   /* TIM2 เป็น Counter 32-bit: นับวนเต็มช่วง */
#define TIM_EGR_UG          (1U << 0U)    /* Update Generation */
#define TIM_CR1_CEN         (1U << 0U)    /* Counter Enable */

/* ตั้งค่า TIM2 ให้เป็น Free-running Counter ที่นับ 1 Tick = 1 microsecond
 * ใช้แทนการหน่วงเวลาแบบ NOP loop เดิม เพราะ DHT11 ต้องการความแม่นยำระดับ us
 * TIM2CLK = 16MHz -> ตั้ง Prescaler = 16 เพื่อให้ได้ Counter Clock = 1MHz (1 Tick = 1us)
 */
void TIM2_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = TIM2_CLOCK_MHZ - 1U;
    TIM2->ARR = TIM2_ARR_MAX;
    TIM2->EGR |= TIM_EGR_UG;   /* บังคับ Update ทันที ให้ค่า Prescaler มีผลจริง */
    TIM2->CR1 |= TIM_CR1_CEN;  /* เริ่มนับ */
}

uint32_t TIM2_GetMicros(void) {
    return TIM2->CNT;
}

/* หน่วงเวลาแบบแม่นยำระดับ microsecond โดยใช้ค่า Counter จริงของ TIM2
 * การลบแบบ Unsigned ทำให้ถูกต้องแม้ Counter จะ Overflow วนกลับมาระหว่างรอ
 */
void TIM2_DelayUs(uint32_t us) {
    uint32_t start = TIM2_GetMicros();

    while ((TIM2_GetMicros() - start) < us) {
        /* รอจนครบเวลา */
    }
}
