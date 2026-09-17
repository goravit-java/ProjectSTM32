#ifndef GPIO_DRIVER_H_
#define GPIO_DRIVER_H_

#include "stm32f411xx_custom.h"

/* LED Pins (Active High) */
#define LED1_PORT   GPIOA
#define LED1_PIN    5       /* D13 Blue */

#define LED2_PORT   GPIOA
#define LED2_PIN    6       /* D12 Red */

#define LED3_PORT   GPIOA
#define LED3_PIN    7       /* D11 Yellow */

#define LED4_PORT   GPIOB
#define LED4_PIN    6       /* D10 Green */

/* Button Pins (Active Low) */
#define BTN_UP_PORT     GPIOA
#define BTN_UP_PIN      10      /* D2 */

#define BTN_DOWN_PORT   GPIOB
#define BTN_DOWN_PIN    3       /* D3 */

#define BTN_OK_PORT     GPIOB
#define BTN_OK_PIN      5       /* D4 */

#define BTN_BACK_PORT   GPIOB
#define BTN_BACK_PIN    4       /* D5 */

/* Function Prototypes */
void GPIO_Init(void);
void LED_On(GPIO_TypeDef *GPIOx, uint8_t pin);
void LED_Off(GPIO_TypeDef *GPIOx, uint8_t pin);
void LED_Toggle(GPIO_TypeDef *GPIOx, uint8_t pin);
uint8_t BTN_IsPressed(GPIO_TypeDef *GPIOx, uint8_t pin);

/* ฟังก์ชันทั่วไปสำหรับควบคุมขา GPIO แบบ Dynamic (ใช้กับ Sensor ที่ต้องสลับ Input/Output เช่น DHT11) */
#define GPIO_MODE_INPUT     0U
#define GPIO_MODE_OUTPUT    1U
#define GPIO_MODE_AF        2U
#define GPIO_MODE_ANALOG    3U

#define GPIO_PULL_NONE      0U
#define GPIO_PULL_UP        1U
#define GPIO_PULL_DOWN      2U

void GPIO_SetPinMode(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t mode);
void GPIO_SetPinPull(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t pull);
void GPIO_WritePin(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t state);
uint8_t GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint8_t pin);

#endif /* GPIO_DRIVER_H_ */
