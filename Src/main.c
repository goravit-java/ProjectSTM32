#include "stm32f411xx_custom.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "adc_driver.h"
#include "iwdg_driver.h"
#include "safety.h"
#include "menu.h"
#include "fsm.h"
#include "dht11_driver.h"

/* DHT11 อ่านค่าได้ไม่เร็วกว่า 1 ครั้ง/วินาที จึงอ่านทุก ๆ ~2 วินาทีแทนที่จะอ่านทุกรอบ Loop
 * (Loop หลักวิ่งทุก ~20ms ดังนั้น 100 รอบ Loop โดยประมาณ = 2 วินาที)
 */
#define DHT11_READ_INTERVAL_TICKS   100U

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
    uint32_t dht11_tick = 0U;

    FPU_Enable();  /* ต้องเป็นบรรทัดแรกสุดของ main() เสมอ ก่อนโค้ดส่วนอื่นที่อาจมี float แฝงอยู่ */

    GPIO_Init();
    UART2_Init();
    ADC1_Init();
    DHT11_Init();
    Menu_Init();
    FSM_Init();
    IWDG_Init();   /* ทำเป็นลำดับสุดท้ายของการ Init เสมอ เผื่อ Init ตัวอื่นค้างจะได้โดน Reset */

    UART2_SendString("\r\n========================================\r\n");
    UART2_SendString("   Smart Vending Machine Controller     \r\n");
    UART2_SendString("   Day 4: Simulation, Refactor & Test    \r\n");
    UART2_SendString("========================================\r\n");

    Menu_PrintAll();

    while (1) {
        IWDG_Refresh(); /* เลี้ยง Watchdog ทุกรอบ Loop ป้องกันโปรแกรมค้างแล้วไม่มีใครรู้ */

        FSM_Run();       /* อ่านปุ่ม + ประมวลผล 1 Tick ของ State ปัจจุบัน (ไม่ Block) */

        dht11_tick++;
        if (dht11_tick >= DHT11_READ_INTERVAL_TICKS) {
            DHT11_Data_t dht_data;

            dht11_tick = 0U;

            if (DHT11_Read(&dht_data) != 0U) {
                UART2_SendString("[DHT11] Humidity: ");
                UART2_SendUint(dht_data.humidity);
                UART2_SendString(" %RH | Temp: ");
                if (dht_data.temperature < 0) {
                    UART2_SendString("-");
                    UART2_SendUint((uint32_t)(-(int32_t)dht_data.temperature));
                } else {
                    UART2_SendUint((uint32_t)dht_data.temperature);
                }
                UART2_SendString(" C\r\n");
            } else {
                UART2_SendString("[DHT11] Read failed (timeout/checksum) - check wiring\r\n");
            }
        }

        delay_ms(20);    /* คาบเวลาสุ่มตรวจปุ่ม ~20ms ช่วย Debounce เบื้องต้น และเป็นฐานเวลาให้ FSM/DHT11 นับ Tick */
    }
}
