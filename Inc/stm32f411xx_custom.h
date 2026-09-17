#ifndef STM32F411XX_CUSTOM_H_
#define STM32F411XX_CUSTOM_H_

#include <stdint.h>

/* Base Addresses */
#define AHB1PERIPH_BASE     (0x40020000UL)
#define APB1PERIPH_BASE     (0x40000000UL)

#define RCC_BASE            (AHB1PERIPH_BASE + 0x3800UL)
#define GPIOA_BASE          (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE          (AHB1PERIPH_BASE + 0x0400UL)
#define USART2_BASE         (APB1PERIPH_BASE + 0x4400UL)

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

/* Peripheral Definitions */
#define RCC                 ((RCC_TypeDef *) RCC_BASE)
#define GPIOA               ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *) GPIOB_BASE)
#define USART2              ((USART_TypeDef *) USART2_BASE)

#endif /* STM32F411XX_CUSTOM_H_ */
