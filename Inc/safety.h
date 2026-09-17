#ifndef SAFETY_H_
#define SAFETY_H_

#include "stm32f411xx_custom.h"

/* เงื่อนไขความปลอดภัย: อุณหภูมิต้องไม่เกิน 30 องศา (ตามเอกสารภาพรวมโครงงาน) */
#define SAFETY_TEMP_MAX_C   (30.0f)

/* คืนค่า 1 = ปลอดภัย (ทำงานต่อได้), 0 = ไม่ปลอดภัย (ต้องเข้า State FAULT) */
uint8_t Safety_CheckTemperature(float current_temp_c);

#endif /* SAFETY_H_ */
