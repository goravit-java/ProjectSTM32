#include "dht11_driver.h"
#include "gpio_driver.h"
#include "tim2_driver.h"

/* Timeout ของการรอสัญญาณแต่ละช่วง (หน่วย us) กันค้างถ้า Sensor ไม่ตอบสนอง/ต่อสายผิด
 * สัญญาณจริงของ DHT11 อยู่ราว 50-80us ต่อช่วง ตั้ง Margin ไว้ให้เผื่อความคลาดเคลื่อน
 */
#define DHT11_TIMEOUT_US   150U

/* เกณฑ์แยกบิต 0/1: ถ้าช่วง HIGH ยาวเกินค่านี้ถือเป็นบิต 1 (~70us), สั้นกว่าถือเป็นบิต 0 (~26-28us) */
#define DHT11_BIT_THRESHOLD_US   40U

static uint8_t DHT11_WaitForLevel(uint8_t level, uint32_t timeout_us);
static uint8_t DHT11_ReadByte(void);

void DHT11_Init(void) {
    RCC->AHB1ENR |= (1U << 2); /* เปิด Clock ให้ GPIOC (PC3) */
    TIM2_Init();

    /* สถานะปกติเมื่อไม่ได้สื่อสาร: Input + Pull-up (บัสถูกดึงขึ้น HIGH ค้างไว้) */
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(DHT11_PORT, DHT11_PIN, GPIO_PULL_UP);
}

/* รอจนกว่าขาจะขึ้น/ลงตามที่ต้องการ ภายในเวลา timeout_us ไม่เช่นนั้นคืนค่า 0 (ล้มเหลว) */
static uint8_t DHT11_WaitForLevel(uint8_t level, uint32_t timeout_us) {
    uint32_t start = TIM2_GetMicros();

    while (GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level) {
        if ((TIM2_GetMicros() - start) > timeout_us) {
            return 0U;
        }
    }
    return 1U;
}

/* อ่านทีละ Byte (8 บิต, MSB ก่อน) ตาม Protocol ของ DHT11
 * ทุกบิตเริ่มด้วย LOW ~50us เสมอ แล้ววัดความยาวช่วง HIGH ที่ตามมาเพื่อแยกว่าเป็นบิต 0 หรือ 1
 */
static uint8_t DHT11_ReadByte(void) {
    uint8_t value = 0U;
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        uint32_t high_start;
        uint32_t high_duration;

        (void)DHT11_WaitForLevel(0U, DHT11_TIMEOUT_US); /* ช่วง LOW เริ่มต้นบิต (~50us) */
        (void)DHT11_WaitForLevel(1U, DHT11_TIMEOUT_US); /* เข้าสู่ช่วง HIGH ที่บอกค่าบิต */

        high_start = TIM2_GetMicros();
        (void)DHT11_WaitForLevel(0U, DHT11_TIMEOUT_US); /* รอจน HIGH จบ (กลับไป LOW ของบิตถัดไป) */
        high_duration = TIM2_GetMicros() - high_start;

        value = (uint8_t)(value << 1);
        if (high_duration > DHT11_BIT_THRESHOLD_US) {
            value |= 1U;
        }
    }
    return value;
}

uint8_t DHT11_Read(DHT11_Data_t *out) {
    uint8_t raw[5] = { 0U, 0U, 0U, 0U, 0U };
    uint8_t checksum;
    uint8_t i;

    /* 1. ส่งสัญญาณ Start: MCU ดึงขาลง LOW อย่างน้อย 18ms */
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_OUTPUT);
    GPIO_WritePin(DHT11_PORT, DHT11_PIN, 0U);
    TIM2_DelayUs(18000U);

    /* 2. ปล่อยขาขึ้น HIGH สั้น ๆ (20-40us) แล้วสลับเป็น Input รอฟัง Sensor ตอบกลับ */
    GPIO_WritePin(DHT11_PORT, DHT11_PIN, 1U);
    TIM2_DelayUs(30U);
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_INPUT);

    /* 3. รอ Response จาก Sensor: LOW ~80us แล้วตามด้วย HIGH ~80us
     *    จบสองช่วงนี้แล้วจะเข้าสู่บิตข้อมูลตัวแรกทันที (ห้ามรอ LOW เพิ่มอีกช่วง มิเช่นนั้นบิตจะเลื่อน)
     */
    if (DHT11_WaitForLevel(0U, DHT11_TIMEOUT_US) == 0U) {
        return 0U; /* Sensor ไม่ตอบสนอง: ตรวจสายไฟ/Pull-up Resistor */
    }
    if (DHT11_WaitForLevel(1U, DHT11_TIMEOUT_US) == 0U) {
        return 0U;
    }

    /* 4. อ่านข้อมูล 5 Byte: Humidity Int, Humidity Dec, Temp Int, Temp Dec, Checksum */
    for (i = 0U; i < 5U; i++) {
        raw[i] = DHT11_ReadByte();
    }

    /* 5. ตรวจ Checksum: ผลรวม 4 Byte แรก (8 บิตล่าง) ต้องตรงกับ Byte ที่ 5 */
    checksum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    if (checksum != raw[4]) {
        return 0U; /* ข้อมูลผิดพลาด (สัญญาณรบกวน/Timing คลาดเคลื่อน) ลองอ่านใหม่ */
    }

    out->humidity = raw[0];
    out->temperature = (int8_t)raw[2];

    return 1U;
}
