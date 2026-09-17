#include "gpio_driver.h"

void GPIO_Init(void) {
    /* 1. เปิด Clock จ่ายไฟให้ GPIOA และ GPIOB */
    RCC->AHB1ENR |= (1U << 0) | (1U << 1);

    /* 2. ตั้งค่า LED เป็น Output (PA5, PA6, PA7, PB6) */
    GPIOA->MODER &= ~((3U << (5*2)) | (3U << (6*2)) | (3U << (7*2)));
    GPIOA->MODER |=  ((1U << (5*2)) | (1U << (6*2)) | (1U << (7*2)));

    GPIOB->MODER &= ~(3U << (6*2));
    GPIOB->MODER |=  (1U << (6*2));

    /* ดับ LED ทั้งหมดก่อนเริ่มงาน */
    LED_Off(LED1_PORT, LED1_PIN);
    LED_Off(LED2_PORT, LED2_PIN);
    LED_Off(LED3_PORT, LED3_PIN);
    LED_Off(LED4_PORT, LED4_PIN);

    /* 3. ตั้งค่า ปุ่มกด เป็น Input + Pull-up (PA10, PB3, PB5, PB4) */
    GPIOA->MODER &= ~(3U << (10*2));                         // PA10 = Input
    GPIOA->PUPDR &= ~(3U << (10*2));
    GPIOA->PUPDR |=  (1U << (10*2));                         // Pull-up

    GPIOB->MODER &= ~((3U << (3*2)) | (3U << (4*2)) | (3U << (5*2))); // PB3, PB4, PB5 = Input
    GPIOB->PUPDR &= ~((3U << (3*2)) | (3U << (4*2)) | (3U << (5*2)));
    GPIOB->PUPDR |=  ((1U << (3*2)) | (1U << (4*2)) | (1U << (5*2))); // Pull-up
}

void LED_On(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->BSRR = (1U << pin);
}

void LED_Off(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->BSRR = (1U << (pin + 16));
}

void LED_Toggle(GPIO_TypeDef *GPIOx, uint8_t pin) {
    GPIOx->ODR ^= (1U << pin);
}

uint8_t BTN_IsPressed(GPIO_TypeDef *GPIOx, uint8_t pin) {
    return ((GPIOx->IDR & (1U << pin)) == 0) ? 1 : 0;
}
