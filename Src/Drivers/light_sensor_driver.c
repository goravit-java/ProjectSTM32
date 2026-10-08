#include "Drivers/light_sensor_driver.h"
#include "Drivers/gpio_driver.h"
#include "Drivers/exti_driver.h"

void LightSensor_Init(void) {
    uint8_t edge;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* ขา DO ของโมดูลมีตัวต้านทาน Pull-up บนบอร์ดอยู่แล้ว จึงตั้งเป็น Input ธรรมดา ไม่ต้องดึงขึ้น/ลงเพิ่ม */
    GPIO_SetPinMode(LIGHT_SENSOR_PORT, LIGHT_SENSOR_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(LIGHT_SENSOR_PORT, LIGHT_SENSOR_PIN, GPIO_PULL_NONE);

    /* ขอบ "เริ่มถูกบัง": ถ้าบังแล้วเป็น HIGH ใช้ขอบขาขึ้น ถ้าบังแล้วเป็น LOW ใช้ขอบขาลง */
    if (LIGHT_BLOCKED_LEVEL != 0U) {
        edge = EXTI_EDGE_RISING;
    } else {
        edge = EXTI_EDGE_FALLING;
    }
    EXTI_InitEdge(LIGHT_SENSOR_PORT, LIGHT_SENSOR_PIN, edge);
}

uint8_t LightSensor_TakeBlockEvent(void) {
    uint8_t blocked = 0U;

    if (EXTI_TakeEvent(LIGHT_SENSOR_PIN) != 0U) {
        /* ยืนยันระดับขาหลังสัญญาณนิ่ง: กรองขอบหลอกที่เกิดตอนเอามือออก (สัญญาณแกว่งช่วงเปลี่ยนความสว่าง) */
        if (GPIO_ReadPin(LIGHT_SENSOR_PORT, LIGHT_SENSOR_PIN) == LIGHT_BLOCKED_LEVEL) {
            blocked = 1U;
        } else {
            /* ไม่ได้บังอยู่จริง: ไม่นับ */
        }
    } else {
        /* ไม่มี Event จากเซ็นเซอร์ */
    }
    return blocked;
}
