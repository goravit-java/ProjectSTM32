#ifndef CORTEX_DRIVER_H_
#define CORTEX_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* เปิด Hardware FPU ของ Cortex-M4 (ต้องเรียกเป็นอย่างแรกสุดใน main() ก่อนใช้ float ใด ๆ) */
void Cortex_FpuEnable(void);

/* เปิดใช้งาน Interrupt หมายเลข irqn ใน NVIC (ดูหมายเลข IRQN ใน stm32f411xx_custom.h) */
void Cortex_NvicEnableIrq(uint8_t irqn);

#endif /* CORTEX_DRIVER_H_ */
