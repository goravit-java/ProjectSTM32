#ifndef DHT11_DRIVER_H_
#define DHT11_DRIVER_H_

#include "stm32f411xx_custom.h"

/* ขาที่ใช้ต่อสาย DATA ของ DHT11 — แก้ตรงนี้ได้ถ้าเปลี่ยนไปใช้ขาอื่น */
#define DHT11_PORT   GPIOC
#define DHT11_PIN    3U   /* PC3 (ขาบน ST Morpho Connector CN7 ของ NUCLEO-F411RE ไม่ใช่ Arduino Header) */

typedef struct {
    uint8_t humidity;    /* หน่วย %RH (0-100) */
    int8_t  temperature; /* หน่วย องศาเซลเซียส (DHT11 อ่านได้เฉพาะจำนวนเต็ม) */
} DHT11_Data_t;

void DHT11_Init(void);

/* อ่านค่าจาก Sensor 1 ครั้ง คืนค่า 1 = สำเร็จ (Checksum ถูกต้อง), 0 = ล้มเหลว (Timeout/Checksum ผิด)
 * หมายเหตุ: DHT11 อ่านค่าได้ไม่เร็วกว่า 1 ครั้ง/วินาที ห้ามเรียกถี่กว่านี้
 */
uint8_t DHT11_Read(DHT11_Data_t *out);

#endif /* DHT11_DRIVER_H_ */
