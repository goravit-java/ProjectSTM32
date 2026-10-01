#include "Drivers/i2c_driver.h"

/* จำนวนรอบหน่วงเวลาครึ่งคาบสัญญาณนาฬิกา (ที่ 16MHz ได้ราว ๆ 1.5-2.5us ต่อครั้ง -> SCL ประมาณ 150-250kHz)
 * SSD1306 รองรับได้ถึง 400kHz จึงมี Margin เหลือเฟือ ถ้าจอแสดงผลเพี้ยนเพราะสายยาว ให้เพิ่มค่านี้
 */
#define SI2C_DELAY_LOOPS    4U

static void SI2C_Delay(void);
static void SI2C_SclHigh(void);
static void SI2C_SclLow(void);
static void SI2C_SdaHigh(void);
static void SI2C_SdaLow(void);
static uint8_t SI2C_SdaRead(void);
static void SI2C_Start(void);
static void SI2C_Stop(void);
static uint8_t SI2C_WriteByte(uint8_t data);

void SoftI2C_Init(void) {
    uint8_t i;

    /* 1. เปิด Clock ให้ GPIOC */
    RCC->AHB1ENR |= (1U << 2);

    /* 2. PC8 (SCL), PC6 (SDA) = General Purpose Output แบบ Open-Drain + Pull-up + High Speed
     * Open-Drain: เขียน 1 = ปล่อยขาลอย (ตัว Pull-up ดึงขึ้น HIGH), เขียน 0 = ดึงลง GND
     * ทำให้อ่านค่า SDA กลับได้จาก IDR ทั้งที่ยังเป็น Output อยู่ (ใช้ตอนรอ ACK จาก Slave)
     */
    SI2C_PORT->MODER &= ~((3U << (SI2C_SCL_PIN * 2U)) | (3U << (SI2C_SDA_PIN * 2U)));
    SI2C_PORT->MODER |=  ((1U << (SI2C_SCL_PIN * 2U)) | (1U << (SI2C_SDA_PIN * 2U)));

    SI2C_PORT->OTYPER |= (1U << SI2C_SCL_PIN) | (1U << SI2C_SDA_PIN);

    SI2C_PORT->OSPEEDR |= (3U << (SI2C_SCL_PIN * 2U)) | (3U << (SI2C_SDA_PIN * 2U));

    SI2C_PORT->PUPDR &= ~((3U << (SI2C_SCL_PIN * 2U)) | (3U << (SI2C_SDA_PIN * 2U)));
    SI2C_PORT->PUPDR |=  ((1U << (SI2C_SCL_PIN * 2U)) | (1U << (SI2C_SDA_PIN * 2U)));

    /* 3. ปล่อย Bus ให้อยู่ในสถานะว่าง (ทั้งสองเส้น HIGH) */
    SI2C_SdaHigh();
    SI2C_SclHigh();
    SI2C_Delay();

    /* 4. Bus Recovery: ถ้า MCU ถูก Reset (เช่นโดน IWDG) กลางคันระหว่างส่งข้อมูล จอ OLED อาจค้างดึง SDA ไว้
     * ส่ง Clock 9 ลูกเพื่อให้ Slave ส่งบิตที่ค้างอยู่จนจบแล้วปล่อย SDA จากนั้นปิดด้วย Stop
     */
    for (i = 0U; i < 9U; i++) {
        SI2C_SclLow();
        SI2C_Delay();
        SI2C_SclHigh();
        SI2C_Delay();
    }
    SI2C_Stop();
}

uint8_t SoftI2C_WriteBytes(uint8_t dev_addr7, const uint8_t *data, uint16_t len) {
    uint16_t i;

    SI2C_Start();

    /* ส่ง Address 7-bit + Write bit (bit0 = 0) */
    if (SI2C_WriteByte((uint8_t)(((uint32_t)dev_addr7 << 1U) & 0xFEU)) == 0U) {
        SI2C_Stop(); /* Slave ไม่ตอบ -> เลิกส่ง (จอไม่ได้ต่อ/Address ผิด) */
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        if (SI2C_WriteByte(data[i]) == 0U) {
            SI2C_Stop();
            return 0U;
        }
    }

    SI2C_Stop();
    return 1U;
}

static void SI2C_Delay(void) {
    for (volatile uint32_t i = 0U; i < SI2C_DELAY_LOOPS; i++) {
        /* หน่วงเวลาสั้น ๆ (volatile กันไม่ให้ Compiler ตัด Loop ทิ้งตอน Optimize) */
    }
}

/* เขียนผ่าน BSRR (Atomic): บิตล่าง 16 บิต = Set HIGH, บิตบน 16 บิต = Reset LOW */
static void SI2C_SclHigh(void) { SI2C_PORT->BSRR = (1U << SI2C_SCL_PIN); }
static void SI2C_SclLow(void)  { SI2C_PORT->BSRR = (1U << (SI2C_SCL_PIN + 16U)); }
static void SI2C_SdaHigh(void) { SI2C_PORT->BSRR = (1U << SI2C_SDA_PIN); }
static void SI2C_SdaLow(void)  { SI2C_PORT->BSRR = (1U << (SI2C_SDA_PIN + 16U)); }

static uint8_t SI2C_SdaRead(void) {
    return ((SI2C_PORT->IDR & (1U << SI2C_SDA_PIN)) != 0U) ? 1U : 0U;
}

/* Start Condition: SDA ตกจาก HIGH -> LOW ขณะที่ SCL ยัง HIGH อยู่ */
static void SI2C_Start(void) {
    SI2C_SdaHigh();
    SI2C_SclHigh();
    SI2C_Delay();
    SI2C_SdaLow();
    SI2C_Delay();
    SI2C_SclLow();
    SI2C_Delay();
}

/* Stop Condition: SDA ขึ้นจาก LOW -> HIGH ขณะที่ SCL HIGH อยู่ */
static void SI2C_Stop(void) {
    SI2C_SdaLow();
    SI2C_Delay();
    SI2C_SclHigh();
    SI2C_Delay();
    SI2C_SdaHigh();
    SI2C_Delay();
}

/* ส่ง 1 Byte (MSB ก่อน) แล้วอ่าน ACK บิตที่ 9 กลับมา คืนค่า 1 = Slave ตอบ ACK, 0 = NACK */
static uint8_t SI2C_WriteByte(uint8_t data) {
    uint8_t bit;
    uint8_t ack;
    uint8_t shift_reg = data; /* MISRA 17.8: เลื่อนบิตในสำเนา ไม่แก้ Parameter */

    for (bit = 0U; bit < 8U; bit++) {
        if ((shift_reg & 0x80U) != 0U) {
            SI2C_SdaHigh();
        } else {
            SI2C_SdaLow();
        }
        SI2C_Delay();
        SI2C_SclHigh();   /* Slave อ่านค่า SDA ตอน SCL เป็น HIGH */
        SI2C_Delay();
        SI2C_SclLow();
        shift_reg = (uint8_t)(shift_reg << 1U);
    }

    /* บิตที่ 9 (ACK): ปล่อย SDA ให้ Slave เป็นฝ่ายดึงลง LOW ถ้ารับข้อมูลได้ */
    SI2C_SdaHigh();
    SI2C_Delay();
    SI2C_SclHigh();
    SI2C_Delay();
    ack = (SI2C_SdaRead() == 0U) ? 1U : 0U;
    SI2C_SclLow();
    SI2C_Delay();

    return ack;
}
