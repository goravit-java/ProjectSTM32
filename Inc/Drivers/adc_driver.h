#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* ADC1 แบบ Interrupt-driven อ่าน 2 Channel บนบอร์ด STEO:
 *   - PA0 (ADC1_IN0) NTC Thermistor วัดอุณหภูมิ  วงจร: 3.3V -- R 10k -- PA0 -- NTC 10k (B = 3950) -- GND
 *   - PA4 (ADC1_IN4) Potentiometer ปุ่มหมุนตั้งเกณฑ์อุณหภูมิ (0-4095)
 * ขั้นตอนการใช้งาน (ไม่มี Polling):
 *   1. ADC1_StartConversion() สั่งเริ่มแปลง NTC แล้วคืนค่าทันที
 *   2. ADC_IRQHandler (EOC Interrupt) เก็บค่า NTC แล้วสลับไปแปลง Pot ต่อเอง เสร็จแล้วเก็บค่า Pot
 *   3. ADC1_HasData() / ADC1_IsSensorOk() / ADC1_GetTemperature() / ADC1_GetPotRaw() อ่านผลล่าสุด
 * หมายเหตุ: ต้องเรียก GPIO_Init() ก่อน ADC1_Init()
 */
void ADC1_Init(void);
void ADC1_StartConversion(void);
uint8_t ADC1_HasData(void);        /* 1 = แปลงครบทั้ง 2 Channel อย่างน้อย 1 รอบแล้ว */
uint8_t ADC1_IsSensorOk(void);     /* 1 = ค่าล่าสุดอยู่ในช่วงใช้งาน, 0 = NTC หลุด/ลัดวงจร */
float ADC1_GetTemperature(void);   /* อุณหภูมิ (°C) จากผลการแปลงล่าสุด (ใช้เมื่อ ADC1_IsSensorOk() = 1) */
uint16_t ADC1_GetPotRaw(void);     /* ค่าดิบของ Potentiometer 0-4095 จากผลการแปลงล่าสุด */

#endif /* ADC_DRIVER_H_ */
