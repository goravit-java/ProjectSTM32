#include "Drivers/stm32f411xx_custom.h"
#include "Drivers/cortex_driver.h"
#include "Drivers/tim2_driver.h"
#include "Drivers/gpio_driver.h"
#include "Drivers/uart_driver.h"
#include "Drivers/adc_driver.h"
#include "Drivers/iwdg_driver.h"
#include "Drivers/dht11_driver.h"
#include "App/safety.h"
#include "App/menu.h"
#include "App/fsm.h"
#include "App/display.h"

/* คาบเวลาของ Main Loop 1 รอบ (1 Tick) = 20 ms คุมด้วย TIM2 ให้แม่นยำ
 * FSM และ Display นับเวลาเป็นจำนวน Tick จากค่านี้
 */
#define MAIN_LOOP_PERIOD_US       20000U

/* DHT11 อ่านค่าได้ไม่เร็วกว่า 1 ครั้ง/วินาที จึงเป็นตัวกำหนดคาบเวลาของ Background Task
 * ตรวจสอบสภาวะแวดล้อม (Temp + Humid) ทุก 100 Tick = 2 วินาที
 */
#define ENV_READ_INTERVAL_TICKS   100U

int main(void) {
    uint32_t env_tick = 0U;
    uint32_t loop_start;
    float mcu_temp = 0.0f;
    /* ค่าความชื้นล่าสุดที่อ่านสำเร็จ ใช้ทดแทนชั่วคราวถ้า DHT11 อ่านพลาดบางรอบ
     * เริ่มต้นที่ 0 (ต่ำกว่าเกณฑ์ 50%) เพื่อไม่ให้ตีความผิดว่าเกินเกณฑ์ก่อนมีข้อมูลจริง
     */
    uint8_t last_humidity = 0U;

    Cortex_FpuEnable();  /* ต้องเป็นบรรทัดแรกสุดของ main() เสมอ ก่อนโค้ดส่วนอื่นที่อาจมี float แฝงอยู่ */

    TIM2_Init();         /* ฐานเวลา us: ต้องมาก่อน ADC1_Init, DHT11, EXTI (Debounce) และ Main Loop */
    GPIO_Init();
    BTN_EnableInterrupts();
    UART2_Init();
    ADC1_Init();
    DHT11_Init();
    Safety_Init();       /* ตั้งค่า LED1-3 และขา PC2 เริ่มต้น (ต้องมาหลัง GPIO_Init เสมอ) */
    Display_Init();      /* ตั้งค่าจอ OLED SSD1306 (Software I2C: SCL=PC8, SDA=PC6) */
    Menu_Init();
    FSM_Init();
    IWDG_Init();         /* ทำเป็นลำดับสุดท้ายของการ Init เสมอ เผื่อ Init ตัวอื่นค้างจะได้โดน Reset */

    UART2_SendString("\r\n========================================\r\n");
    UART2_SendString(" Smart Vegetable & Fruit Vending Machine \r\n");
    UART2_SendString("  Environment Monitoring & Lockout Mode  \r\n");
    UART2_SendString("========================================\r\n");

    Menu_PrintAll();

    for (;;) {
        loop_start = TIM2_GetMicros();

        IWDG_Refresh();   /* เลี้ยง Watchdog ทุกรอบ Loop ป้องกันโปรแกรมค้างแล้วไม่มีใครรู้ */

        FSM_Run();        /* รับ Event ปุ่ม (จาก EXTI) + ประมวลผล 1 Tick ของ State ปัจจุบัน (ไม่ Block) */

        Display_Update(); /* วาดหน้าจอ OLED ตาม State/Lockout ปัจจุบัน (หน่วงความถี่ Refresh เองภายใน) */

        env_tick++;

        /* สั่ง ADC เริ่มแปลงล่วงหน้า 1 Tick (20 ms) ผลจะถูกเก็บโดย ADC_IRQHandler ทันทีที่แปลงเสร็จ
         * พอถึง Tick ถัดไปค่าก็พร้อมใช้ โดยไม่ต้องวนรอ (ไม่มี Polling)
         */
        if (env_tick == (ENV_READ_INTERVAL_TICKS - 1U)) {
            ADC1_StartConversion();
        }

        if (env_tick >= ENV_READ_INTERVAL_TICKS) {
            DHT11_Data_t dht_data;

            env_tick = 0U;

            /* อุณหภูมิ: ผลจาก Internal Temperature Sensor ของ ADC1 ที่ Interrupt เก็บไว้ */
            if (ADC1_HasData() != 0U) {
                mcu_temp = ADC1_GetTemperature();
            }

            /* ความชื้น: อ่านจาก DHT11 เท่านั้นตามที่โครงงานกำหนด (ไม่ใช้ค่า Temp ที่ DHT11 อ่านได้)
             * ถ้าอ่านพลาด (Timeout/Checksum) ให้คงค่าความชื้นล่าสุดที่เคยอ่านได้ไว้ก่อน
             */
            if (DHT11_Read(&dht_data) != 0U) {
                last_humidity = dht_data.humidity;
            } else {
                UART2_SendString("[DHT11] Read failed (timeout/checksum) - check wiring\r\n");
            }

            /* ส่งเข้า safety.c ที่เดียว: WARNING + คุม LED1-3/PC2 + ติดตาม Lockout */
            if (ADC1_HasData() != 0U) {
                Safety_Update(mcu_temp, last_humidity);
            }
        }

        /* รอให้ครบคาบ 20 ms นับจากต้นรอบ (ถ้างานรอบนี้ใช้เวลาเกินแล้วจะไม่รอเพิ่ม) */
        while ((TIM2_GetMicros() - loop_start) < MAIN_LOOP_PERIOD_US) {
            /* ว่าง: รอจังหวะ Tick ถัดไป (Interrupt ของ UART/ADC/EXTI ยังทำงานตามปกติระหว่างนี้) */
        }
    }
}
