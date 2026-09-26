#include "stm32f411xx_custom.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "adc_driver.h"
#include "iwdg_driver.h"
#include "safety.h"
#include "menu.h"
#include "fsm.h"
#include "dht11_driver.h"

/* DHT11 อ่านค่าได้ไม่เร็วกว่า 1 ครั้ง/วินาที จึงเป็นตัวกำหนดคาบเวลาของ Background Task
 * ตรวจสอบสภาวะแวดล้อมทั้งหมด (Temp + Humid) ไว้ที่ ~2 วินาทีต่อครั้ง แทนที่จะอ่านทุกรอบ Loop
 * (Loop หลักวิ่งทุก ~20ms ดังนั้น 100 รอบ Loop โดยประมาณ = 2 วินาที)
 */
#define ENV_READ_INTERVAL_TICKS   100U

static void delay_ms(volatile uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 3000; i++) {
        __asm("NOP");
    }
}

/* เปิดใช้งาน Hardware FPU ของ Cortex-M4 (ต้องเรียกก่อนมีการใช้ float/double ใด ๆ ทั้งหมด)
 * ให้สิทธิ์ Full Access กับ Coprocessor 10 และ 11 (บิต 20-23 ของ CPACR)
 * ตามด้วย DSB/ISB เพื่อให้แน่ใจว่าการตั้งค่ามีผลก่อนคำสั่งถัดไปจะถูกดึงมา Execute
 */
static void FPU_Enable(void) {
    SCB_CPACR |= (0xFU << 20);
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

int main(void) {
    uint32_t env_tick = 0U;
    /* ค่าความชื้นล่าสุดที่อ่านสำเร็จ ใช้ทดแทนชั่วคราวถ้า DHT11 อ่านพลาดบางรอบ
     * เริ่มต้นที่ 0 (ต่ำกว่าเกณฑ์ 50%) เพื่อไม่ให้ตีความผิดว่าเกินเกณฑ์ก่อนมีข้อมูลจริง
     */
    uint8_t last_humidity = 0U;

    FPU_Enable();  /* ต้องเป็นบรรทัดแรกสุดของ main() เสมอ ก่อนโค้ดส่วนอื่นที่อาจมี float แฝงอยู่ */

    GPIO_Init();
    UART2_Init();
    ADC1_Init();
    DHT11_Init();
    Safety_Init();  /* ตั้งค่า LED1-3 เริ่มต้น (ต้องมาหลัง GPIO_Init เสมอ) */
    Menu_Init();
    FSM_Init();
    IWDG_Init();   /* ทำเป็นลำดับสุดท้ายของการ Init เสมอ เผื่อ Init ตัวอื่นค้างจะได้โดน Reset */

    UART2_SendString("\r\n========================================\r\n");
    UART2_SendString(" Smart Vegetable & Fruit Vending Machine \r\n");
    UART2_SendString("  Environment Monitoring & Lockout Mode  \r\n");
    UART2_SendString("========================================\r\n");

    Menu_PrintAll();

    while (1) {
        IWDG_Refresh(); /* เลี้ยง Watchdog ทุกรอบ Loop ป้องกันโปรแกรมค้างแล้วไม่มีใครรู้ */

        FSM_Run();       /* อ่านปุ่ม + ประมวลผล 1 Tick ของ State ปัจจุบัน (ไม่ Block) */

        env_tick++;
        if (env_tick >= ENV_READ_INTERVAL_TICKS) {
            DHT11_Data_t dht_data;
            float mcu_temp;

            env_tick = 0U;

            /* อุณหภูมิ: อ่านจาก Internal Temperature Sensor ของ ADC1 เสมอ (ไม่ผูกกับผล DHT11) */
            mcu_temp = ADC1_ReadTemperature();

            /* ความชื้น: อ่านจาก DHT11 เท่านั้นตามที่โครงงานกำหนด (ไม่ใช้ค่า Temp ที่ DHT11 อ่านได้)
             * ถ้าอ่านพลาด (Timeout/Checksum) ให้คงค่าความชื้นล่าสุดที่เคยอ่านได้ไว้ก่อน
             * เพื่อไม่ให้การเชื่อมต่อ DHT11 หลุดชั่วคราวทำให้หยุดตรวจสอบอุณหภูมิไปด้วย
             */
            if (DHT11_Read(&dht_data) != 0U) {
                last_humidity = dht_data.humidity;
            } else {
                UART2_SendString("[DHT11] Read failed (timeout/checksum) - check wiring\r\n");
            }

            /* ส่งเข้า safety.c ที่เดียว: พิมพ์สถานะ/WARNING + คุม LED1-3 + ติดตาม Lockout */
            Safety_Update(mcu_temp, last_humidity);
        }

        delay_ms(20);    /* คาบเวลาสุ่มตรวจปุ่ม ~20ms ช่วย Debounce เบื้องต้น และเป็นฐานเวลาให้ FSM/Env Task นับ Tick */
    }
}
