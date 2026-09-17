#ifndef IWDG_DRIVER_H_
#define IWDG_DRIVER_H_

#include "stm32f411xx_custom.h"

typedef struct {
    volatile uint32_t KR, PR, RLR, SR;
} IWDG_TypeDef;

#define IWDG_BASE   (0x40003000UL)
#define IWDG        ((IWDG_TypeDef *) IWDG_BASE)

void IWDG_Init(void);
void IWDG_Refresh(void);

#endif
