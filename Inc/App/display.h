#ifndef DISPLAY_H_
#define DISPLAY_H_

/* เรียกครั้งเดียวตอนเริ่มระบบ (หลัง GPIO_Init) เพื่อตั้งค่าจอ OLED SSD1306 (GME12864-78) */
void Display_Init(void);

/* เรียกทุกรอบของ Main Loop: ภายในจะหน่วงความถี่การ Refresh จอเอง (ดู DISPLAY_REFRESH_TICKS ใน display.c)
 * เพื่อไม่ให้การส่งข้อมูลเต็มจอผ่าน I2C ไปหน่วงการอ่านปุ่ม/การทำงานของ FSM มากเกินไป
 */
void Display_Update(void);

#endif /* DISPLAY_H_ */
