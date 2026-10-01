#ifndef STM32F411XX_CUSTOM_H_
#define STM32F411XX_CUSTOM_H_

#include <stdint.h>

/* Base Addresses */
#define AHB1PERIPH_BASE     (0x40020000UL)
#define APB1PERIPH_BASE     (0x40000000UL)
#define APB2PERIPH_BASE     (0x40010000UL)

#define RCC_BASE            (AHB1PERIPH_BASE + 0x3800UL)
#define GPIOA_BASE          (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE          (AHB1PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE          (AHB1PERIPH_BASE + 0x0800UL)
#define USART2_BASE         (APB1PERIPH_BASE + 0x4400UL)
#define ADC1_BASE           (APB2PERIPH_BASE + 0x2000UL)
#define ADC_COMMON_BASE     (APB2PERIPH_BASE + 0x2300UL)
#define IWDG_BASE            (0x40003000UL)   /* IWDG อยู่บน APB1 bus เสมอ ไม่ผูกกับ Clock Enable ใด ๆ */
#define TIM2_BASE            (APB1PERIPH_BASE + 0x0000UL)
#define SYSCFG_BASE          (APB2PERIPH_BASE + 0x3800UL)  /* เลือกว่า EXTI Line ไหนผูกกับ Port ไหน */
#define EXTI_BASE            (APB2PERIPH_BASE + 0x3C00UL)
#define NVIC_ISER_BASE       (0xE000E100UL)                /* Cortex-M4 Core: NVIC Interrupt Set-Enable */

/* RCC Register Structure */
typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR, AHB1RSTR, AHB2RSTR;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t APB1RSTR, APB2RSTR;
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR, AHB2ENR;
    volatile uint32_t RESERVED2[2];
    volatile uint32_t APB1ENR, APB2ENR;
} RCC_TypeDef;

/* GPIO Register Structure */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
} GPIO_TypeDef;

/* USART Register Structure */
typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_TypeDef;

/* ADC Register Structure */
typedef struct {
    volatile uint32_t SR, CR1, CR2, SMPR1, SMPR2;
    volatile uint32_t JOFR1, JOFR2, JOFR3, JOFR4;
    volatile uint32_t HTR, LTR;
    volatile uint32_t SQR1, SQR2, SQR3;
    volatile uint32_t JSQR;
    volatile uint32_t JDR1, JDR2, JDR3, JDR4;
    volatile uint32_t DR;
} ADC_TypeDef;

/* ADC Common Register Structure (ควบคุมค่าร่วมของ ADC เช่น Temp Sensor/VREFINT) */
typedef struct {
    volatile uint32_t CSR, CCR, CDR;
} ADC_Common_TypeDef;

/* Independent Watchdog (IWDG) Register Structure */
typedef struct {
    volatile uint32_t KR, PR, RLR, SR, WINR;
} IWDG_TypeDef;

/* General-Purpose Timer Register Structure (ใช้กับ TIM2 สำหรับจับเวลาระดับ Microsecond) */
typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR;
    volatile uint32_t RESERVED0;
    volatile uint32_t CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t RESERVED1;
    volatile uint32_t DCR, DMAR, OR;
} TIM_TypeDef;

/* System Configuration Controller (ใช้แค่ EXTICR สำหรับเลือก Port ของ EXTI แต่ละ Line) */
typedef struct {
    volatile uint32_t MEMRMP, PMC;
    volatile uint32_t EXTICR[4];
} SYSCFG_TypeDef;

/* External Interrupt/Event Controller */
typedef struct {
    volatile uint32_t IMR, EMR, RTSR, FTSR, SWIER, PR;
} EXTI_TypeDef;

/* NVIC Interrupt Set-Enable Registers (ISER0-ISER7, เขียน 1 = เปิด IRQ นั้น, เขียน 0 = ไม่มีผล) */
typedef struct {
    volatile uint32_t ISER[8];
} NVIC_ISER_TypeDef;

/* Peripheral Definitions */
#define RCC                 ((RCC_TypeDef *) RCC_BASE)
#define GPIOA               ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOC               ((GPIO_TypeDef *) GPIOC_BASE)
#define USART2              ((USART_TypeDef *) USART2_BASE)
#define ADC1                ((ADC_TypeDef *) ADC1_BASE)
#define ADC123_COMMON       ((ADC_Common_TypeDef *) ADC_COMMON_BASE)
#define IWDG                ((IWDG_TypeDef *) IWDG_BASE)
#define TIM2                ((TIM_TypeDef *) TIM2_BASE)
#define SYSCFG              ((SYSCFG_TypeDef *) SYSCFG_BASE)
#define EXTI                ((EXTI_TypeDef *) EXTI_BASE)
#define NVIC_ISER           ((NVIC_ISER_TypeDef *) NVIC_ISER_BASE)

/* IRQ Number ของ STM32F411 (ตำแหน่งใน Vector Table หลัง 16 Exception ของ Core) */
#define EXTI3_IRQN          9U
#define EXTI4_IRQN          10U
#define ADC_IRQN            18U
#define EXTI9_5_IRQN        23U
#define USART2_IRQN         38U
#define EXTI15_10_IRQN      40U

/* Cortex-M4 Core Peripheral: SCB->CPACR (Coprocessor Access Control Register)
 * ต้องเปิดใช้งาน FPU (Coprocessor 10, 11) ก่อนใช้ float/double ใด ๆ ในโปรแกรมเสมอ
 * ไม่เช่นนั้นคำสั่ง Floating-point แรกที่เจอจะทำให้เกิด Hard Fault ทันที
 * (ที่อยู่ Register นี้ตายตัวสำหรับ Cortex-M4 ทุกรุ่น ไม่ผูกกับ Vendor)
 */
#define SCB_CPACR           (*((volatile uint32_t *) 0xE000ED88UL))

#endif /* STM32F411XX_CUSTOM_H_ */
