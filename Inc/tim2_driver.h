#ifndef TIM2_DRIVER_H_
#define TIM2_DRIVER_H_

#include "stm32f411xx_custom.h"

void TIM2_Init(void);
uint32_t TIM2_GetMicros(void);
void TIM2_DelayUs(uint32_t us);

#endif /* TIM2_DRIVER_H_ */
