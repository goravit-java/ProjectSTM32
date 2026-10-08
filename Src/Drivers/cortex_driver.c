#include "Drivers/cortex_driver.h"

/* MISRA C:2012 Directive 4.3: คำสั่ง Assembly ต้องถูกห่อไว้ในฟังก์ชันที่มีหน้าที่ชัดเจนเท่านั้น
 * ไฟล์นี้จึงเป็นที่เดียวในโปรเจกต์ที่มี Inline Assembly (DSB/ISB จำเป็นสำหรับการเปิด FPU)
 */
void Cortex_FpuEnable(void) {
    SCB_CPACR |= SCB_CPACR_CP10_CP11_FULL;  /* CP10, CP11 = Full Access (บิต 20-23) */
    __asm volatile ("dsb");                 /* รอให้การเขียน Register เสร็จจริง */
    __asm volatile ("isb");                 /* ล้าง Pipeline ให้คำสั่งถัดไปเห็นค่าที่ตั้งใหม่ */
}

void Cortex_NvicEnableIrq(uint8_t irqn) {
    uint32_t index = (uint32_t)irqn / NVIC_IRQS_PER_REG;
    uint32_t bit = (uint32_t)irqn % NVIC_IRQS_PER_REG;

    if (index < NVIC_ISER_COUNT) {
        NVIC_ISER->ISER[index] = (1U << bit); /* ISER เขียน 1 = เปิด, เขียน 0 = ไม่มีผล (ไม่ต้อง Read-Modify-Write) */
    } else {
        /* หมายเลข IRQ เกินช่วงของ STM32F411: ไม่ทำอะไร */
    }
}
