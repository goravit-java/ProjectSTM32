#include "adc_driver.h"

void ADC1_Init(void) {
    /* 1. เปิด Clock ให้ ADC1 (Bit 8 ใน APB2ENR) */
    RCC->APB2ENR |= (1U << 8);

    /* 2. จ่ายไฟให้ Internal Temp Sensor (Bit 23 ใน ADC_CCR) */
    ADC_CCR |= (1U << 23);

    /* 3. ตั้งค่า Sampling Time สำหรับ Channel 18 ให้สูงสุด (480 cycles) เพื่อความแม่นยำ */
    ADC1->SMPR1 |= (7U << 24);

    /* 4. กำหนดให้ Sequence 1 อ่านค่าจาก Channel 18 (Temp Sensor) */
    ADC1->SQR3 &= ~(0x1FU);
    ADC1->SQR3 |= 18U;

    /* 5. เปิดใช้งานโมดูล ADC1 (ADON = Bit 0) */
    ADC1->CR2 |= (1U << 0);
}

uint16_t ADC1_ReadRaw(void) {
    /* เพิ่ม Delay เล็กน้อยให้ ADC1 Stabilize ก่อนเริ่มแปลง */
    for (volatile int i = 0; i < 1000; i++);

    /* สั่งเริ่มแปลงสัญญาณ (SWSTART = Bit 30) */
    ADC1->CR2 |= (1U << 30);

    /* รอจนกว่าจะแปลงสัญญาณเสร็จ (EOC = Bit 1) พร้อมระบบ Timeout */
    uint32_t timeout = 100000;
    while (!(ADC1->SR & (1U << 1))) {
        timeout--;
        if (timeout == 0) {
            return 0; // หมดเวลารอ ให้คืนค่า 0 เพื่อไม่ให้ MCU ค้าง
        }
    }

    /* คืนค่าดิจิทัล 12-bit จาก Data Register */
    return (uint16_t)(ADC1->DR & 0xFFFF);
}

float ADC1_ReadTemperature(void) {
    uint16_t raw_adc = ADC1_ReadRaw();

    // หากอ่านค่าไม่ได้
    if (raw_adc == 0) {
        return -999.0f;
    }

    // แปลงค่า Raw ADC เป็น แรงดันไฟฟ้า (Volts) อ้างอิง VREF = 3.3V
    float voltage = ((float)raw_adc * 3.3f) / 4095.0f;

    // คำนวณอุณหภูมิ (°C) ตาม Datasheet ของ STM32F4
    float temp = ((voltage - 0.76f) / 0.0025f) + 25.0f;

    return temp;
}
