#include "safety.h"
#include "uart_driver.h"

uint8_t Safety_CheckTemperature(float current_temp_c) {
    if (current_temp_c > SAFETY_TEMP_MAX_C) {
        UART2_SendString("[SAFETY] WARNING: MCU temperature exceeds safe limit (30 C)!\r\n");
        return 0U; // ไม่ปลอดภัย -> ผู้เรียกต้องเปลี่ยน State ไปที่ FAULT
    }

    return 1U; // ปลอดภัย -> ทำงานต่อได้ตามปกติ
}
