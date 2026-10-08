#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* ADC1 อ่านอุณหภูมิจาก NTC Thermistor บนบอร์ด STEO ที่ขา PA0 (ADC1_IN0) แบบ Interrupt-driven
 * วงจร: 3.3V -- R 10k -- PA0 -- NTC 10k (B = 3950) -- GND
 * ขั้นตอนการใช้งาน (ไม่มี Polling):
 *   1. ADC1_StartConversion() สั่งเริ่มแปลงค่า แล้วคืนค่าทันที
 *   2. เมื่อแปลงเสร็จ ADC_IRQHandler (EOC Interrupt) จะเก็บผลไว้ให้อัตโนมัติ
 *   3. ADC1_HasData() / ADC1_IsSensorOk() / ADC1_GetTemperature() อ่านผลล่าสุดที่ ISR เก็บไว้
 * หมายเหตุ: ต้องเรียก GPIO_Init() ก่อน ADC1_Init()
 */
void ADC1_Init(void);
void ADC1_StartConversion(void);
uint8_t ADC1_HasData(void);        /* 1 = มีผลการแปลงอย่างน้อย 1 ครั้งแล้ว */
uint8_t ADC1_IsSensorOk(void);     /* 1 = ค่าล่าสุดอยู่ในช่วงใช้งาน, 0 = NTC หลุด/ลัดวงจร */
float ADC1_GetTemperature(void);   /* อุณหภูมิ (°C) จากผลการแปลงล่าสุด (ใช้เมื่อ ADC1_IsSensorOk() = 1) */

#endif /* ADC_DRIVER_H_ */
