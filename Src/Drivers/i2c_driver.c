#include "Drivers/i2c_driver.h"

/* จำนวนรอบหน่วงเวลาครึ่งคาบสัญญาณนาฬิกา (ที่ 16MHz ได้ราว ๆ 1.5-2.5us ต่อครั้ง -> SCL ประมาณ 150-250kHz)
 * SSD1306 รองรับได้ถึง 400kHz จึงมี Margin เหลือเฟือ ถ้าจอแสดงผลเพี้ยนเพราะสายยาว ให้เพิ่มค่านี้
 */
#define SI2C_DELAY_LOOPS        4U

#define SI2C_GPIO_MODE_OUTPUT   0x1U
#define SI2C_GPIO_PULL_UP       0x1U
#define SI2C_RECOVERY_CLOCKS    9U      /* Clock 9 ลูก = 8 บิตข้อมูล + 1 บิต ACK ที่อาจค้างอยู่ */
#define SI2C_BITS_PER_BYTE      8U
#define SI2C_MSB_MASK           0x80U
#define SI2C_WRITE_ADDR_MASK    0xFEU   /* บิต 0 = 0 หมายถึงโหมดเขียน (Write) */
#define LEVEL_LOW               0U

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
    uint32_t scl_shift = SI2C_SCL_PIN * GPIO_MODER_BITS_PER_PIN;
    uint32_t sda_shift = SI2C_SDA_PIN * GPIO_MODER_BITS_PER_PIN;

    /* 1. เปิด Clock ให้ GPIOC */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    /* 2. PC8 (SCL), PC6 (SDA) = General Purpose Output แบบ Open-Drain + Pull-up + High Speed
     * Open-Drain: เขียน 1 = ปล่อยขาลอย (ตัว Pull-up ดึงขึ้น HIGH), เขียน 0 = ดึงลง GND
     * ทำให้อ่านค่า SDA กลับได้จาก IDR ทั้งที่ยังเป็น Output อยู่ (ใช้ตอนรอ ACK จาก Slave)
     */
    SI2C_PORT->MODER &= ~((GPIO_MODER_FIELD_MASK << scl_shift) | (GPIO_MODER_FIELD_MASK << sda_shift));
    SI2C_PORT->MODER |= ((SI2C_GPIO_MODE_OUTPUT << scl_shift) | (SI2C_GPIO_MODE_OUTPUT << sda_shift));

    SI2C_PORT->OTYPER |= ((1U << SI2C_SCL_PIN) | (1U << SI2C_SDA_PIN));

    SI2C_PORT->OSPEEDR |= ((GPIO_OSPEED_VERY_HIGH << scl_shift) | (GPIO_OSPEED_VERY_HIGH << sda_shift));

    SI2C_PORT->PUPDR &= ~((GPIO_MODER_FIELD_MASK << scl_shift) | (GPIO_MODER_FIELD_MASK << sda_shift));
    SI2C_PORT->PUPDR |= ((SI2C_GPIO_PULL_UP << scl_shift) | (SI2C_GPIO_PULL_UP << sda_shift));

    /* 3. ปล่อย Bus ให้อยู่ในสถานะว่าง (ทั้งสองเส้น HIGH) */
    SI2C_SdaHigh();
    SI2C_SclHigh();
    SI2C_Delay();

    /* 4. Bus Recovery: ถ้า MCU ถูก Reset (เช่นโดน IWDG) กลางคันระหว่างส่งข้อมูล จอ OLED อาจค้างดึง SDA ไว้
     * ส่ง Clock 9 ลูกเพื่อให้ Slave ส่งบิตที่ค้างอยู่จนจบแล้วปล่อย SDA จากนั้นปิดด้วย Stop
     */
    for (i = 0U; i < SI2C_RECOVERY_CLOCKS; i++) {
        SI2C_SclLow();
        SI2C_Delay();
        SI2C_SclHigh();
        SI2C_Delay();
    }
    SI2C_Stop();
}

uint8_t SoftI2C_WriteBytes(uint8_t dev_addr7, const uint8_t *data, uint16_t len) {
    uint16_t i;
    uint8_t ok;

    SI2C_Start();

    /* ส่ง Address 7-bit + Write bit (bit0 = 0) ถ้า Slave ไม่ตอบ ACK (จอไม่ได้ต่อ/Address ผิด) จะไม่ส่งข้อมูลต่อ */
    ok = SI2C_WriteByte((uint8_t)(((uint32_t)dev_addr7 << 1U) & SI2C_WRITE_ADDR_MASK));

    for (i = 0U; (i < len) && (ok != 0U); i++) {
        ok = SI2C_WriteByte(data[i]);
    }

    SI2C_Stop();
    return ok;
}

static void SI2C_Delay(void) {
    volatile uint32_t i;

    for (i = 0U; i < SI2C_DELAY_LOOPS; i++) {
        /* หน่วงเวลาสั้น ๆ (volatile กันไม่ให้ Compiler ตัด Loop ทิ้งตอน Optimize) */
    }
}

/* เขียนผ่าน BSRR (Atomic): บิตล่าง 16 บิต = Set HIGH, บิตบน 16 บิต = Reset LOW */
static void SI2C_SclHigh(void) {
    SI2C_PORT->BSRR = (1U << SI2C_SCL_PIN);
}

static void SI2C_SclLow(void) {
    SI2C_PORT->BSRR = (1U << (SI2C_SCL_PIN + GPIO_BSRR_RESET_OFFSET));
}

static void SI2C_SdaHigh(void) {
    SI2C_PORT->BSRR = (1U << SI2C_SDA_PIN);
}

static void SI2C_SdaLow(void) {
    SI2C_PORT->BSRR = (1U << (SI2C_SDA_PIN + GPIO_BSRR_RESET_OFFSET));
}

static uint8_t SI2C_SdaRead(void) {
    uint8_t level;

    if ((SI2C_PORT->IDR & (1U << SI2C_SDA_PIN)) != 0U) {
        level = 1U;
    } else {
        level = 0U;
    }
    return level;
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
    uint8_t shift_reg = data; /* เลื่อนบิตในสำเนา ไม่แก้ค่า Parameter โดยตรง */

    for (bit = 0U; bit < SI2C_BITS_PER_BYTE; bit++) {
        if ((shift_reg & SI2C_MSB_MASK) != 0U) {
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
    if (SI2C_SdaRead() == LEVEL_LOW) {
        ack = 1U;
    } else {
        ack = 0U;
    }
    SI2C_SclLow();
    SI2C_Delay();

    return ack;
}
