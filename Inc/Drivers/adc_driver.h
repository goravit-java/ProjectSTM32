#ifndef ADC_DRIVER_H_
#define ADC_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* ADC1 อ่าน Internal Temperature Sensor (Channel 18) แบบ Interrupt-driven
 * ขั้นตอนการใช้งาน (ไม่มี Polling):
 *   1. ADC1_StartConversion() สั่งเริ่มแปลงค่า แล้วคืนค่าทันที
 *   2. เมื่อแปลงเสร็จ ADC_IRQHandler (EOC Interrupt) จะเก็บผลไว้ให้อัตโนมัติ
 *   3. ADC1_HasData() / ADC1_GetTemperature() อ่านผลล่าสุดที่ ISR เก็บไว้
 * หมายเหตุ: ต้องเรียก TIM2_Init() ก่อน ADC1_Init() (ใช้หน่วงเวลารอ Sensor พร้อม)
 */
void ADC1_Init(void);
void ADC1_StartConversion(void);
uint8_t ADC1_HasData(void);        /* 1 = มีผลการแปลงอย่างน้อย 1 ครั้งแล้ว */
float ADC1_GetTemperature(void);   /* อุณหภูมิ (°C) จากผลการแปลงล่าสุด */

#endif /* ADC_DRIVER_H_ */
