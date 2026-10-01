#include "Drivers/gpio_driver.h"
#include "Drivers/exti_driver.h"

void GPIO_Init(void) {
    /* 1. เปิด Clock จ่ายไฟให้ GPIOA และ GPIOB */
    RCC->AHB1ENR |= (1U << 0U) | (1U << 1U);

    /* 2. ตั้งค่า LED เป็น Output (PA5, PA6, PA7, PB6) */
    GPIO_SetPinMode(LED1_PORT, LED1_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetPinMode(LED2_PORT, LED2_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetPinMode(LED3_PORT, LED3_PIN, GPIO_MODE_OUTPUT);
    GPIO_SetPinMode(LED4_PORT, LED4_PIN, GPIO_MODE_OUTPUT);

    /* ดับ LED ทั้งหมดก่อนเริ่มงาน */
    LED_Off(LED1_PORT, LED1_PIN);
    LED_Off(LED2_PORT, LED2_PIN);
    LED_Off(LED3_PORT, LED3_PIN);
    LED_Off(LED4_PORT, LED4_PIN);

    /* 3. ตั้งค่า ปุ่มกด เป็น Input + Pull-up (PA10, PB3, PB5, PB4) */
    GPIO_SetPinMode(BTN_UP_PORT, BTN_UP_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(BTN_UP_PORT, BTN_UP_PIN, GPIO_PULL_UP);

    GPIO_SetPinMode(BTN_DOWN_PORT, BTN_DOWN_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(BTN_DOWN_PORT, BTN_DOWN_PIN, GPIO_PULL_UP);

    GPIO_SetPinMode(BTN_OK_PORT, BTN_OK_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(BTN_OK_PORT, BTN_OK_PIN, GPIO_PULL_UP);

    GPIO_SetPinMode(BTN_BACK_PORT, BTN_BACK_PIN, GPIO_MODE_INPUT);
    GPIO_SetPinPull(BTN_BACK_PORT, BTN_BACK_PIN, GPIO_PULL_UP);
}

void LED_On(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->BSRR = (1U << (uint32_t)pin);
}

void LED_Off(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->BSRR = (1U << ((uint32_t)pin + 16U));
}

void LED_Toggle(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->ODR ^= (1U << (uint32_t)pin);
}

uint8_t BTN_IsPressed(const GPIO_TypeDef *GPIOx, uint8_t pin) {
    return ((GPIOx->IDR & (1U << (uint32_t)pin)) == 0U) ? 1U : 0U;
}

void BTN_EnableInterrupts(void) {
    EXTI_InitFallingEdge(BTN_UP_PORT, BTN_UP_PIN);
    EXTI_InitFallingEdge(BTN_DOWN_PORT, BTN_DOWN_PIN);
    EXTI_InitFallingEdge(BTN_OK_PORT, BTN_OK_PIN);
    EXTI_InitFallingEdge(BTN_BACK_PORT, BTN_BACK_PIN);
}

uint8_t BTN_TakePress(const GPIO_TypeDef *GPIOx, uint8_t pin) {
    uint8_t pressed = 0U;

    if (EXTI_TakeEvent(pin) != 0U) {
        pressed = BTN_IsPressed(GPIOx, pin);
    }
    return pressed;
}

/* ตั้งค่า Mode ของขา (Input/Output/AF/Analog) ทีละขา ใช้เวลาต้องสลับ Direction แบบ Dynamic
 * เช่น DHT11 ที่ต้องเป็น Output ตอนส่ง Start Signal แล้วสลับเป็น Input ตอนอ่านข้อมูลกลับ
 */
void GPIO_SetPinMode(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t mode) {
    GPIOx->MODER &= ~(3U << ((uint32_t)pin * 2U));
    GPIOx->MODER |= ((uint32_t)mode << ((uint32_t)pin * 2U));
}

/* ตั้งค่า Pull-up/Pull-down ของขา (0=ไม่ต่อ, 1=Pull-up, 2=Pull-down) */
void GPIO_SetPinPull(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t pull) {
    GPIOx->PUPDR &= ~(3U << ((uint32_t)pin * 2U));
    GPIOx->PUPDR |= ((uint32_t)pull << ((uint32_t)pin * 2U));
}

/* เขียนค่า Digital Output ของขา (state: 0=Low, 1=High) ผ่าน BSRR (Atomic, ปลอดภัยกว่าการแก้ ODR ตรง ๆ) */
void GPIO_WritePin(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t state) {
    if (state != 0U) {
        GPIOx->BSRR = (1U << (uint32_t)pin);
    } else {
        GPIOx->BSRR = (1U << ((uint32_t)pin + 16U));
    }
}

/* อ่านค่า Digital Input ของขา คืนค่า 0 หรือ 1 */
uint8_t GPIO_ReadPin(const GPIO_TypeDef *GPIOx, uint8_t pin) {
    return ((GPIOx->IDR & (1U << (uint32_t)pin)) != 0U) ? 1U : 0U;
}
