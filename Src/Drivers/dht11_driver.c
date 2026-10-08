#include "Drivers/dht11_driver.h"
#include "Drivers/gpio_driver.h"
#include "Drivers/tim2_driver.h"

/* Timeout ของการรอสัญญาณแต่ละช่วง (หน่วย us) กันค้างถ้า Sensor ไม่ตอบสนอง/ต่อสายผิด
 * สัญญาณจริงของ DHT11 อยู่ราว 50-80us ต่อช่วง ตั้ง Margin ไว้ให้เผื่อความคลาดเคลื่อน
 */
#define DHT11_TIMEOUT_US         150U

/* เกณฑ์แยกบิต 0/1: ถ้าช่วง HIGH ยาวเกินค่านี้ถือเป็นบิต 1 (~70us), สั้นกว่าถือเป็นบิต 0 (~26-28us) */
#define DHT11_BIT_THRESHOLD_US   40U

#define DHT11_START_LOW_US       18000U  /* Start Signal: MCU ดึงขาลง LOW อย่างน้อย 18 ms */
#define DHT11_START_RELEASE_US   30U     /* ปล่อยขา HIGH 20-40 us ก่อนสลับเป็น Input */
#define DHT11_BITS_PER_BYTE      8U
#define DHT11_FRAME_BYTES        5U      /* Humid Int, Humid Dec, Temp Int, Temp Dec, Checksum */

/* ตำแหน่ง Byte ในเฟรมข้อมูล */
#define DHT11_IDX_HUMID_INT      0U
#define DHT11_IDX_HUMID_DEC      1U
#define DHT11_IDX_TEMP_INT       2U
#define DHT11_IDX_TEMP_DEC       3U
#define DHT11_IDX_CHECKSUM       4U

#define LEVEL_LOW                0U
#define LEVEL_HIGH               1U

static uint8_t DHT11_WaitForLevel(uint8_t level, uint32_t timeout_us);
static uint8_t DHT11_ReadByte(void);

void DHT11_Init(void) {
    /* หมายเหตุ: TIM2_Init() ถูกเรียกไว้แล้วใน main() ก่อนหน้า (ใช้ร่วมกันหลายโมดูล) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; /* เปิด Clock ให้ GPIOC (PC3) */

    /* สถานะปกติเมื่อไม่ได้สื่อสาร: Input + Pull-up (บัสถูกดึงขึ้น HIGH ค้างไว้) */
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(DHT11_PORT, DHT11_PIN, GPIO_PULL_UP);
}

/* รอจนกว่าขาจะขึ้น/ลงตามที่ต้องการ ภายในเวลา timeout_us คืนค่า 1 = สำเร็จ, 0 = หมดเวลา */
static uint8_t DHT11_WaitForLevel(uint8_t level, uint32_t timeout_us) {
    uint32_t start = TIM2_GetMicros();
    uint8_t ok = 1U;

    while ((ok != 0U) && (GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level)) {
        if ((TIM2_GetMicros() - start) > timeout_us) {
            ok = 0U;
        } else {
            /* ยังไม่หมดเวลา: รอต่อ */
        }
    }
    return ok;
}

/* อ่านทีละ Byte (8 บิต, MSB ก่อน) ตาม Protocol ของ DHT11
 * ทุกบิตเริ่มด้วย LOW ~50us เสมอ แล้ววัดความยาวช่วง HIGH ที่ตามมาเพื่อแยกว่าเป็นบิต 0 หรือ 1
 */
static uint8_t DHT11_ReadByte(void) {
    uint8_t value = 0U;
    uint8_t i;

    for (i = 0U; i < DHT11_BITS_PER_BYTE; i++) {
        uint32_t high_start;
        uint32_t high_duration;

        (void)DHT11_WaitForLevel(LEVEL_LOW, DHT11_TIMEOUT_US);  /* ช่วง LOW เริ่มต้นบิต (~50us) */
        (void)DHT11_WaitForLevel(LEVEL_HIGH, DHT11_TIMEOUT_US); /* เข้าสู่ช่วง HIGH ที่บอกค่าบิต */

        high_start = TIM2_GetMicros();
        (void)DHT11_WaitForLevel(LEVEL_LOW, DHT11_TIMEOUT_US);  /* รอจน HIGH จบ (กลับไป LOW ของบิตถัดไป) */
        high_duration = TIM2_GetMicros() - high_start;

        value = (uint8_t)(value << 1U);
        if (high_duration > DHT11_BIT_THRESHOLD_US) {
            value |= 1U;
        } else {
            /* บิต 0: ไม่ต้องเติมบิต */
        }
    }
    return value;
}

uint8_t DHT11_Read(DHT11_Data_t *out) {
    uint8_t raw[DHT11_FRAME_BYTES] = {
        0U, 0U, 0U, 0U, 0U
    };
    uint8_t checksum;
    uint8_t i;
    uint8_t result = 0U;

    /* 1. ส่งสัญญาณ Start: MCU ดึงขาลง LOW อย่างน้อย 18ms */
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_OUTPUT);
    GPIO_WritePin(DHT11_PORT, DHT11_PIN, LEVEL_LOW);
    TIM2_DelayUs(DHT11_START_LOW_US);

    /* 2. ปล่อยขาขึ้น HIGH สั้น ๆ (20-40us) แล้วสลับเป็น Input รอฟัง Sensor ตอบกลับ */
    GPIO_WritePin(DHT11_PORT, DHT11_PIN, LEVEL_HIGH);
    TIM2_DelayUs(DHT11_START_RELEASE_US);
    GPIO_SetPinMode(DHT11_PORT, DHT11_PIN, GPIO_MODE_INPUT);

    /* 3. รอ Response จาก Sensor: LOW ~80us แล้วตามด้วย HIGH ~80us
     *    จบสองช่วงนี้แล้วจะเข้าสู่บิตข้อมูลตัวแรกทันที (ห้ามรอ LOW เพิ่มอีกช่วง มิเช่นนั้นบิตจะเลื่อน)
     */
    if ((DHT11_WaitForLevel(LEVEL_LOW, DHT11_TIMEOUT_US) != 0U) &&
        (DHT11_WaitForLevel(LEVEL_HIGH, DHT11_TIMEOUT_US) != 0U)) {

        /* 4. อ่านข้อมูล 5 Byte: Humidity Int, Humidity Dec, Temp Int, Temp Dec, Checksum */
        for (i = 0U; i < DHT11_FRAME_BYTES; i++) {
            raw[i] = DHT11_ReadByte();
        }

        /* 5. ตรวจ Checksum: ผลรวม 4 Byte แรก (8 บิตล่าง) ต้องตรงกับ Byte ที่ 5 */
        checksum = (uint8_t)(((uint32_t)raw[DHT11_IDX_HUMID_INT] + (uint32_t)raw[DHT11_IDX_HUMID_DEC])
                             + ((uint32_t)raw[DHT11_IDX_TEMP_INT] + (uint32_t)raw[DHT11_IDX_TEMP_DEC]));
        if (checksum == raw[DHT11_IDX_CHECKSUM]) {
            out->humidity = raw[DHT11_IDX_HUMID_INT];
            out->temperature = (int8_t)raw[DHT11_IDX_TEMP_INT];
            result = 1U;
        } else {
            /* Checksum ผิด (สัญญาณรบกวน/Timing คลาดเคลื่อน): คืนค่า 0 ให้ผู้เรียกลองอ่านใหม่รอบหน้า */
        }
    } else {
        /* Sensor ไม่ตอบสนอง: ตรวจสายไฟ/Pull-up Resistor */
    }

    return result;
}
