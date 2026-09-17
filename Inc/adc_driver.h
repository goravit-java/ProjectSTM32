#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "stm32f411xx_custom.h"

/* Register Structure สำหรับ ADC1 */
typedef struct {
    volatile uint32_t SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4;
    volatile uint32_t HTR, LTR, SQR1, SQR2, SQR3, JSQR, JDR1, JDR2, JDR3, JDR4, DR;
} ADC_TypeDef;

#define ADC1_BASE      (0x40012000UL)
#define ADC1           ((ADC_TypeDef *) ADC1_BASE)

/* Register ADC Common Control (ADC_CCR) สำหรับเปิด Temp Sensor */
#define ADC_CCR_BASE   (0x40012300UL)
#define ADC_CCR        (*((volatile uint32_t *)(ADC_CCR_BASE + 0x04)))

/* Prototypes */
void ADC1_Init(void);
uint16_t ADC1_ReadRaw(void);
float ADC1_ReadTemperature(void);

#endif /* ADC_DRIVER_H_ */
