#include <math.h>
#include "Drivers/adc_driver.h"
#include "Drivers/cortex_driver.h"
#include "Drivers/gpio_driver.h"

/* ขาที่ต่อกับ NTC บนบอร์ด STEO: PA0 = ADC1_IN0 */
#define NTC_PORT            GPIOA
#define NTC_PIN             0U
#define ADC_CHANNEL_NTC     0U

/* ขาที่ต่อกับ Potentiometer บนบอร์ด STEO: PA4 = ADC1_IN4 (ปุ่มหมุนตั้งเกณฑ์อุณหภูมิ) */
#define POT_PORT            GPIOA
#define POT_PIN             4U
#define ADC_CHANNEL_POT     4U

/* ค่าคงที่ของวงจรแบ่งแรงดันและ NTC (ตรงกับ Lab 4.2 ของบอร์ด STEO)
 * R_ntc = R_SERIES * raw / (ADC_FULL_SCALE - raw)  (Vref = Vcc = 3.3V จึงตัดกันหมด)
 * สมการ Beta: 1/T = 1/T0 + ln(R_ntc / R0) / BETA   (T เป็น Kelvin)
 */
#define NTC_SERIES_OHM      (10000.0f)   /* ตัวต้านทานคงที่ฝั่ง 3.3V */
#define NTC_R0_OHM          (10000.0f)   /* ความต้านทาน NTC ที่ 25°C */
#define NTC_T0_KELVIN       (298.15f)    /* 25°C ในหน่วย Kelvin */
#define NTC_BETA            (3950.0f)
#define KELVIN_OFFSET       (273.15f)
#define ADC_FULL_SCALE_F    (4095.0f)    /* 12-bit */

/* ช่วงค่า raw ที่ถือว่า NTC ต่ออยู่ปกติ (ประมาณ -60°C ถึง 220°C)
 * raw ชิด 0    = NTC ลัดวงจรลง GND
 * raw ชิด 4095 = NTC หลุด/ไม่ได้ต่อ (ถ้าไม่ตรวจ จะคำนวณได้อุณหภูมิต่ำมาก ระบบจะไม่ล็อกทั้งที่ไม่มีเซ็นเซอร์)
 */
#define ADC_RAW_VALID_MIN   20U
#define ADC_RAW_VALID_MAX   4075U

#define ADC_SR_EOC          (1U << 1U)
#define ADC_CR1_EOCIE       (1U << 5U)
#define ADC_CR1_RES_MASK    (0x3U << 24U)   /* RES[1:0] = 00 -> 12-bit */
#define ADC_CR2_ADON        (1U << 0U)
#define ADC_CR2_SWSTART     (1U << 30U)
#define ADC_SQR1_L_MASK     (0xFU << 20U)   /* L[3:0] = 0000 -> แปลง 1 Channel */
#define ADC_SQR3_SQ1_MASK   0x1FU
#define ADC_SMPR2_CH0_POS   0U              /* Sample time ของ Channel 0 อยู่ที่ SMPR2[2:0] */
#define ADC_SMPR2_CH4_POS   12U             /* Sample time ของ Channel 4 อยู่ที่ SMPR2[14:12] */
#define ADC_SMPR_FIELD_MASK 0x7U
#define ADC_SMPR_480_CYCLES 0x7U            /* เวลา Sample ยาวสุด รองรับอิมพีแดนซ์ราว 5k ของวงจรแบ่งแรงดัน */
#define ADC_DATA_MASK_12BIT 0x0FFFU

/* ลำดับการแปลง 1 รอบ: NTC (PA0) ก่อน แล้ว ISR สลับไปแปลง Pot (PA4) ต่อเอง */
#define ADC_STEP_IDLE       0U
#define ADC_STEP_NTC        1U
#define ADC_STEP_POT        2U

/* ผลการแปลงล่าสุด เขียนโดย ISR อ่านโดยโปรแกรมหลัก (volatile เพราะเปลี่ยนค่าได้นอกลำดับการทำงานปกติ) */
static volatile uint16_t adc_last_raw = 0U;      /* NTC */
static volatile uint16_t adc_pot_raw = 0U;       /* Potentiometer */
static volatile uint8_t adc_has_data = 0U;
static volatile uint8_t adc_step = ADC_STEP_IDLE;

static void ADC1_SelectChannel(uint32_t channel);

void ADC_IRQHandler(void);

void ADC1_Init(void) {
    /* 1. เปิด Clock ให้ GPIOA (เผื่อไว้ ปกติ GPIO_Init เปิดแล้ว) และ ADC1 (อยู่บน APB2 Bus) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* 2. ตั้งขา PA0 (NTC) และ PA4 (Pot) เป็น Analog Mode ไม่มี Pull-up/Pull-down (ไม่ให้รบกวนวงจรแบ่งแรงดัน) */
    GPIO_SetPinMode(NTC_PORT, NTC_PIN, GPIO_MODE_ANALOG);
    GPIO_SetPinPull(NTC_PORT, NTC_PIN, GPIO_PULL_NONE);
    GPIO_SetPinMode(POT_PORT, POT_PIN, GPIO_MODE_ANALOG);
    GPIO_SetPinPull(POT_PORT, POT_PIN, GPIO_PULL_NONE);

    /* 3. ตั้งค่า Resolution เป็น 12-bit (RES = 00) */
    ADC1->CR1 &= ~ADC_CR1_RES_MASK;

    /* 4. แปลงครั้งละ 1 Channel (L = 0) เริ่มที่ Channel 0 (PA0) แล้ว ISR จะสลับ Channel เองทีละขั้น */
    ADC1->SQR1 &= ~ADC_SQR1_L_MASK;
    ADC1_SelectChannel(ADC_CHANNEL_NTC);

    /* 5. Sample Time ของ Channel 0 และ Channel 4 = 480 cycles ทั้งคู่ */
    ADC1->SMPR2 &= ~(ADC_SMPR_FIELD_MASK << ADC_SMPR2_CH0_POS);
    ADC1->SMPR2 |= (ADC_SMPR_480_CYCLES << ADC_SMPR2_CH0_POS);
    ADC1->SMPR2 &= ~(ADC_SMPR_FIELD_MASK << ADC_SMPR2_CH4_POS);
    ADC1->SMPR2 |= (ADC_SMPR_480_CYCLES << ADC_SMPR2_CH4_POS);

    /* 6. เปิด EOC Interrupt: ADC จะแจ้งเตือนเองเมื่อแปลงเสร็จ แทนการวนรอบิต EOC */
    ADC1->CR1 |= ADC_CR1_EOCIE;
    Cortex_NvicEnableIrq((uint8_t)ADC_IRQN);

    /* 7. เปิดใช้งาน ADC1 (การแปลงครั้งแรกเกิดหลังจากนี้อย่างน้อย 1 รอบ Main Loop ADC พร้อมใช้งานนานแล้ว) */
    ADC1->CR2 |= ADC_CR2_ADON;
}

/* เริ่มแปลง 1 รอบ (NTC แล้วต่อด้วย Pot) ถ้ารอบก่อนหน้ายังไม่จบจะไม่เริ่มซ้อน */
void ADC1_StartConversion(void) {
    if (adc_step == ADC_STEP_IDLE) {
        adc_step = ADC_STEP_NTC;
        ADC1_SelectChannel(ADC_CHANNEL_NTC);
        ADC1->CR2 |= ADC_CR2_SWSTART;
    } else {
        /* รอบก่อนยังแปลงไม่เสร็จ (ใช้เวลาไม่ถึง 0.2 ms ปกติไม่เกิด): ข้ามรอบนี้ไป */
    }
}

/* EOC Interrupt: การอ่าน DR จะเคลียร์บิต EOC ให้อัตโนมัติ
 * ขั้นที่ 1 (NTC) เสร็จ -> เก็บค่า แล้วสลับไป Channel 4 และสั่งแปลงต่อทันที (ไม่ต้องรอ Main Loop)
 * ขั้นที่ 2 (Pot) เสร็จ -> เก็บค่า ตั้งธงว่ามีข้อมูลครบ แล้วกลับสถานะว่าง
 */
void ADC_IRQHandler(void) {
    if ((ADC1->SR & ADC_SR_EOC) != 0U) {
        uint16_t raw = (uint16_t)(ADC1->DR & ADC_DATA_MASK_12BIT);

        if (adc_step == ADC_STEP_NTC) {
            adc_last_raw = raw;
            adc_step = ADC_STEP_POT;
            ADC1_SelectChannel(ADC_CHANNEL_POT);
            ADC1->CR2 |= ADC_CR2_SWSTART;
        } else if (adc_step == ADC_STEP_POT) {
            adc_pot_raw = raw;
            adc_has_data = 1U;
            adc_step = ADC_STEP_IDLE;
        } else {
            /* EOC ที่ไม่ได้สั่ง: อ่าน DR ทิ้งไปแล้ว (เคลียร์ธง) ไม่ทำอะไรต่อ */
        }
    } else {
        /* Interrupt ที่ไม่ใช่ EOC (ไม่ได้เปิดใช้งาน): ไม่ทำอะไร */
    }
}

static void ADC1_SelectChannel(uint32_t channel) {
    ADC1->SQR3 &= ~ADC_SQR3_SQ1_MASK;
    ADC1->SQR3 |= channel;
}

uint16_t ADC1_GetPotRaw(void) {
    return adc_pot_raw;
}

uint8_t ADC1_HasData(void) {
    return adc_has_data;
}

uint8_t ADC1_IsSensorOk(void) {
    uint16_t raw = adc_last_raw;
    uint8_t ok;

    if ((raw >= ADC_RAW_VALID_MIN) && (raw <= ADC_RAW_VALID_MAX)) {
        ok = 1U;
    } else {
        ok = 0U;
    }
    return ok;
}

float ADC1_GetTemperature(void) {
    uint16_t raw = adc_last_raw; /* อ่านสำเนาครั้งเดียว ป้องกันค่าเปลี่ยนกลางการคำนวณ */
    float raw_f;
    float r_ntc;
    float inv_kelvin;

    /* กันหารด้วยศูนย์และ log(0) ถ้าถูกเรียกตอนค่าอยู่นอกช่วง (ปกติผู้เรียกเช็ค ADC1_IsSensorOk() ก่อน) */
    if (raw < ADC_RAW_VALID_MIN) {
        raw = ADC_RAW_VALID_MIN;
    } else if (raw > ADC_RAW_VALID_MAX) {
        raw = ADC_RAW_VALID_MAX;
    } else {
        /* อยู่ในช่วงปกติ */
    }

    raw_f = (float)raw;

    /* 1. ความต้านทาน NTC จากวงจรแบ่งแรงดัน */
    r_ntc = (NTC_SERIES_OHM * raw_f) / (ADC_FULL_SCALE_F - raw_f);

    /* 2. สมการ Beta -> Kelvin -> Celsius */
    inv_kelvin = (1.0f / NTC_T0_KELVIN) + (logf(r_ntc / NTC_R0_OHM) / NTC_BETA);

    return (1.0f / inv_kelvin) - KELVIN_OFFSET;
}
