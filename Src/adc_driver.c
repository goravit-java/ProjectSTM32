#include "adc_driver.h"

/* ค่า Calibration ที่โรงงานเก็บไว้ใน System Memory (เฉพาะ STM32F4 series)
 * TS_CAL1 = ค่า ADC ที่วัดได้ตอนอุณหภูมิ 30°C, Vref+ = 3.3V
 * TS_CAL2 = ค่า ADC ที่วัดได้ตอนอุณหภูมิ 110°C, Vref+ = 3.3V
 * อ้างอิง: RM0383 Reference Manual, Section "Temperature sensor characteristics"
 */
#define TS_CAL1        (*((volatile uint16_t *) 0x1FFF7A2CUL))
#define TS_CAL2        (*((volatile uint16_t *) 0x1FFF7A2EUL))
#define TS_CAL1_TEMP   (30.0f)
#define TS_CAL2_TEMP   (110.0f)

void ADC1_Init(void) {
    /* 1. เปิด Clock ให้ ADC1 (อยู่บน APB2 Bus) */
    RCC->APB2ENR |= (1U << 8); // ADC1EN

    /* 2. ตั้งค่า Resolution เป็น 12-bit (RES = 00, เป็นค่า default อยู่แล้ว แต่เคลียร์ไว้ให้ชัดเจน) */
    ADC1->CR1 &= ~(3U << 24);

    /* 3. เปิดใช้งาน Internal Temperature Sensor + VREFINT ผ่านบิต TSVREFE ใน Common Register */
    ADC123_COMMON->CCR |= (1U << 23); // TSVREFE = 1

    /* 4. ตั้งค่า Sequence ให้แปลงแค่ 1 Channel คือ Channel 18 (Internal Temp Sensor) */
    ADC1->SQR1 &= ~(0xFU << 20);      // L[3:0] = 0000 -> 1 conversion ใน Regular sequence
    ADC1->SQR3 &= ~(0x1FU << 0);
    ADC1->SQR3 |=  (18U << 0);        // SQ1 = Channel 18

    /* 5. ตั้งค่า Sample Time ของ Channel 18 ให้นานที่สุด (480 cycles)
     *    เนื่องจาก Datasheet กำหนดว่า Sampling time ของ Temp Sensor ต้อง > 17.1 us
     *    Channel 18 อยู่ใน SMPR1 บิตตำแหน่ง [26:24]
     */
    ADC1->SMPR1 &= ~(7U << 24);
    ADC1->SMPR1 |=  (7U << 24);       // 111 = 480 cycles

    /* 6. เปิดใช้งาน ADC1 (ADON) */
    ADC1->CR2 |= (1U << 0);

    /* หน่วงเวลาสั้น ๆ รอ Temperature Sensor Stable ก่อนอ่านค่าจริงครั้งแรก (t_START ตาม Datasheet) */
    for (volatile uint32_t i = 0; i < 10000U; i++) {
        __asm("NOP");
    }
}

uint16_t ADC1_ReadRaw(void) {
    ADC1->CR2 |= (1U << 30);              // SWSTART: สั่งเริ่มแปลงค่า
    while (!(ADC1->SR & (1U << 1))) { }   /* รอจน EOC = 1 (End Of Conversion) */
    return (uint16_t)(ADC1->DR & 0xFFFFU);
}

float ADC1_ReadTemperature(void) {
    uint16_t raw = ADC1_ReadRaw();

    /* สูตรตาม Reference Manual:
     * Temp = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / (TS_CAL2 - TS_CAL1)) * (raw - TS_CAL1) + TS_CAL1_TEMP
     */
    float temp = ((TS_CAL2_TEMP - TS_CAL1_TEMP) / (float)(TS_CAL2 - TS_CAL1))
                 * ((float)raw - (float)TS_CAL1)
                 + TS_CAL1_TEMP;

    return temp;
}
