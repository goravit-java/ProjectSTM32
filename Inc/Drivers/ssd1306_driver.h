#ifndef SSD1306_DRIVER_H_
#define SSD1306_DRIVER_H_

#include "Drivers/stm32f411xx_custom.h"

#define SSD1306_I2C_ADDR     0x3CU   /* Address I2C เริ่มต้นของโมดูล GME12864-78 (บางตัวใช้ 0x3D ดูวิธีเช็คในคำอธิบาย) */
#define SSD1306_WIDTH        128U
#define SSD1306_HEIGHT       64U
#define SSD1306_PAGE_COUNT   8U      /* 64 / 8 */

/* เรียกครั้งเดียวตอนเริ่มระบบ: เปิด Software I2C (PC8/PC6) + ส่งลำดับคำสั่งเริ่มต้นจอ + เคลียร์จอให้ว่างสนิท */
void SSD1306_Init(void);

/* ล้าง Framebuffer ในหน่วยความจำให้เป็นสีดับทั้งหมด (ยังไม่ส่งออกจอจริงจนกว่าจะเรียก UpdateScreen) */
void SSD1306_Clear(void);

/* ส่ง Framebuffer ทั้งหมดในหน่วยความจำออกไปวาดที่จอจริงผ่าน I2C (ใช้เวลาหลัก ms ต่อครั้ง) */
void SSD1306_UpdateScreen(void);

/* วาดจุดเดียวลง Framebuffer (color: 1 = สว่าง, 0 = ดับ) ไม่มีผลถ้าพิกัดเกินขอบจอ */
void SSD1306_SetPixel(uint16_t x, uint16_t y, uint8_t color);

/* วาดตัวอักษร/ข้อความ/ตัวเลข/ทศนิยม 1 ตำแหน่งแบบไม่ใช้ sprintf (เบากว่าสำหรับระบบ Bare-metal)
 * ทุกฟังก์ชันที่วาดข้อความ คืนค่าตำแหน่ง x ถัดไปหลังวาดจบ เพื่อนำไปต่อข้อความ/ตัวเลขอื่นได้ทันที
 */
void SSD1306_DrawChar(uint16_t x, uint16_t y, char c);
uint16_t SSD1306_DrawString(uint16_t x, uint16_t y, const char *str);
uint16_t SSD1306_DrawUint(uint16_t x, uint16_t y, uint32_t value);
uint16_t SSD1306_DrawFloat1(uint16_t x, uint16_t y, float value);

/* เครื่องมือวาดรูปทรงพื้นฐาน สำหรับเส้นแบ่ง Header/Footer และ Progress Bar */
void SSD1306_DrawHLine(uint16_t x, uint16_t y, uint16_t length);
void SSD1306_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void SSD1306_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

#endif /* SSD1306_DRIVER_H_ */
