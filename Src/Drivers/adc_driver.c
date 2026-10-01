#include "Drivers/adc_driver.h"
#include "Drivers/cortex_driver.h"
#include "Drivers/tim2_driver.h"

/* ค่า Calibration ที่โรงงานเก็บไว้ใน System Memory (เฉพาะ STM32F4 series)
 * TS_CAL1 = ค่า ADC ที่วัดได้ตอนอุณหภูมิ 30°C, Vref+ = 3.3V
 * TS_CAL2 = ค่า ADC ที่วัดได้ตอนอุณหภูมิ 110°C, Vref+ = 3.3V
 * อ้างอิง: RM0383 Reference Manual, Section "Temperature sensor characteristics"
 */
#define TS_CAL1        (*((volatile uint16_t *) 0x1FFF7A2CUL))
#define TS_CAL2        (*((volatile uint16_t *) 0x1FFF7A2EUL))
#define TS_CAL1_TEMP   (30.0f)
#define TS_CAL2_TEMP   (110.0f)

#define ADC_SR_EOC       (1U << 1U)
#define ADC_CR1_EOCIE    (1U << 5U)
#define ADC_CR2_ADON     (1U << 0U)
#define ADC_CR2_SWSTART  (1U << 30U)

#define ADC_STARTUP_US   100U  /* เผื่อเวลา Temp Sensor เริ่มทำงาน (Datasheet: t_START สูงสุด 10us) */

/* ผลการแปลงล่าสุด เขียนโดย ISR อ่านโดยโปรแกรมหลัก (volatile เพราะเปลี่ยนค่าได้นอกลำดับการทำงานปกติ) */
static volatile uint16_t adc_last_raw = 0U;
static volatile uint8_t adc_has_data = 0U;

void ADC_IRQHandler(void);

void ADC1_Init(void) {
    /* 1. เปิด Clock ให้ ADC1 (อยู่บน APB2 Bus) */
    RCC->APB2ENR |= (1U << 8U); /* ADC1EN */

    /* 2. ตั้งค่า Resolution เป็น 12-bit (RES = 00) */
    ADC1->CR1 &= ~(3U << 24U);

    /* 3. เปิดใช้งาน Internal Temperature Sensor + VREFINT ผ่านบิต TSVREFE ใน Common Register */
    ADC123_COMMON->CCR |= (1U << 23U); /* TSVREFE = 1 */

    /* 4. ตั้งค่า Sequence ให้แปลงแค่ 1 Channel คือ Channel 18 (Internal Temp Sensor) */
    ADC1->SQR1 &= ~(0xFU << 20U);      /* L[3:0] = 0000 -> 1 conversion */
    ADC1->SQR3 &= ~(0x1FU << 0U);
    ADC1->SQR3 |=  (18U << 0U);        /* SQ1 = Channel 18 */

    /* 5. Sample Time ของ Channel 18 = 480 cycles (Datasheet: Temp Sensor ต้อง > 17.1 us)
     *    Channel 18 อยู่ใน SMPR1 บิต [26:24]
     */
    ADC1->SMPR1 &= ~(7U << 24U);
    ADC1->SMPR1 |=  (7U << 24U);

    /* 6. เปิด EOC Interrupt: ADC จะแจ้งเตือนเองเมื่อแปลงเสร็จ แทนการวนรอบิต EOC */
    ADC1->CR1 |= ADC_CR1_EOCIE;
    Cortex_NvicEnableIrq((uint8_t)ADC_IRQN);

    /* 7. เปิดใช้งาน ADC1 แล้วรอ Temperature Sensor พร้อมก่อนแปลงค่าครั้งแรก */
    ADC1->CR2 |= ADC_CR2_ADON;
    TIM2_DelayUs(ADC_STARTUP_US);
}

void ADC1_StartConversion(void) {
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

/* EOC Interrupt: การอ่าน DR จะเคลียร์บิต EOC ให้อัตโนมัติ */
void ADC_IRQHandler(void) {
    if ((ADC1->SR & ADC_SR_EOC) != 0U) {
        adc_last_raw = (uint16_t)(ADC1->DR & 0x0FFFU);
        adc_has_data = 1U;
    }
}

uint8_t ADC1_HasData(void) {
    return adc_has_data;
}

float ADC1_GetTemperature(void) {
    uint16_t raw = adc_last_raw; /* อ่านสำเนาครั้งเดียว ป้องกันค่าเปลี่ยนกลางการคำนวณ */

    /* สูตรตาม Reference Manual:
     * Temp = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / (TS_CAL2 - TS_CAL1)) * (raw - TS_CAL1) + TS_CAL1_TEMP
     */
    float temp = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / ((float)TS_CAL2 - (float)TS_CAL1))
                 * ((float)raw - (float)TS_CAL1)
                 + TS_CAL1_TEMP;

    return temp;
}
