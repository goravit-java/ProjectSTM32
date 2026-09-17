#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "stm32f411xx_custom.h"

void ADC1_Init(void);
uint16_t ADC1_ReadRaw(void);
float ADC1_ReadTemperature(void);

#endif /* ADC_DRIVER_H_ */
