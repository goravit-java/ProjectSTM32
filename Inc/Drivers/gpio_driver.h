#ifndef GPIO_DRIVER_H_
#define GPIO_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

/* LED Pins (Active High)
 * LED1-3 ถูกควบคุมโดย safety.c แบบ Background Task ต่อเนื่อง ไม่ผูกกับ State ของ FSM
 * ส่วน LED4 เป็นหน้าที่ของ fsm.c (Busy/Dispensing)
 */
#define LED1_PORT   GPIOA
#define LED1_PIN    5U      /* D13 Blue   -> NORMAL STATUS (safety.c) */

#define LED2_PORT   GPIOA
#define LED2_PIN    6U      /* D12 Red    -> TEMPERATURE ALARM (safety.c) */

#define LED3_PORT   GPIOA
#define LED3_PIN    7U      /* D11 Yellow -> HUMIDITY ALARM (safety.c) */

#define LED4_PORT   GPIOB
#define LED4_PIN    6U      /* D10 Green  -> DISPENSING/BUSY STATUS (fsm.c) */

/* Button Pins (Active Low, ต่อลง GND, ใช้ Pull-up ภายใน) — ทุกปุ่มทำงานผ่าน EXTI
 * เลขขาต้องไม่ซ้ำกัน เพราะ EXTI Line แต่ละเส้นผูกได้กับขาเลขเดียวกันได้แค่ Port เดียว
 */
#define BTN_UP_PORT     GPIOA
#define BTN_UP_PIN      10U     /* D2 -> EXTI10 */

#define BTN_DOWN_PORT   GPIOB
#define BTN_DOWN_PIN    3U      /* D3 -> EXTI3 */

#define BTN_OK_PORT     GPIOB
#define BTN_OK_PIN      5U      /* D4 -> EXTI5 */

#define BTN_BACK_PORT   GPIOB
#define BTN_BACK_PIN    4U      /* D5 -> EXTI4 */

/* Function Prototypes */
void GPIO_Init(void);
void LED_On(GPIO_TypeDef *GPIOx, uint8_t pin);
void LED_Off(GPIO_TypeDef *GPIOx, uint8_t pin);
void LED_Toggle(GPIO_TypeDef *GPIOx, uint8_t pin);
uint8_t BTN_IsPressed(const GPIO_TypeDef *GPIOx, uint8_t pin);

/* เปิด External Interrupt ให้ปุ่มทั้ง 4 (ต้องเรียกหลัง GPIO_Init และ TIM2_Init) */
void BTN_EnableInterrupts(void);

/* คืนค่า 1 ครั้งเดียวต่อการกด 1 ครั้ง: มี Interrupt จากปุ่มนี้ และตอนนี้ (หลังหน้าสัมผัสนิ่ง) ยังกดอยู่จริง
 * การยืนยันระดับขานี้ช่วยกรอง Noise และขอบขาลงหลอกที่เกิดจากการเด้งตอนปล่อยปุ่ม
 */
uint8_t BTN_TakePress(const GPIO_TypeDef *GPIOx, uint8_t pin);

/* ปุ่ม BACK (PB4) ใช้ EXTI ทั้งขอบขาลง (กด) และขาขึ้น (ปล่อย) เพื่อแยกการกดสั้นกับการกดค้าง
 *   BTN_EVENT_SHORT : ส่งทันทีตอนกดลง (ตอบสนองไวเหมือนปุ่มอื่น)
 *   BTN_EVENT_LONG  : ส่งครั้งเดียวเมื่อกดค้างครบ BTN_LONG_PRESS_US ขณะที่ยังกดอยู่ (ไม่ต้องรอปล่อยมือ)
 * การจับเวลาใช้ TIM2 เทียบกับเวลาที่ Interrupt รับการกด ไม่มีการวนอ่านขาปุ่ม
 */
#define BTN_EVENT_NONE      0U
#define BTN_EVENT_SHORT     1U
#define BTN_EVENT_LONG      2U
#define BTN_LONG_PRESS_US   1500000U   /* 1.5 วินาที */

uint8_t BTN_TakeBackEvent(void);   /* เรียกทุก Tick แทน BTN_TakePress() สำหรับปุ่ม BACK */

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
uint8_t GPIO_ReadPin(const GPIO_TypeDef *GPIOx, uint8_t pin);

#endif /* GPIO_DRIVER_H_ */
