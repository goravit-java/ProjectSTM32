#ifndef UART_DRIVER_H_
#define UART_DRIVER_H_

#include "stm32f411xx_custom.h"

void UART2_Init(void);
void UART2_SendChar(char c);
void UART2_SendString(char *str);
void UART2_SendUint(uint32_t value);
void UART2_PrintTemperature(float temp);

#endif /* UART_DRIVER_H_ */
