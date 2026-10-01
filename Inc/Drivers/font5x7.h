#ifndef FONT5X7_H_
#define FONT5X7_H_

#include <stdint.h>

#define FONT_FIRST_CHAR   0x20U   /* ASCII ตัวแรกที่ตารางรองรับ (space) */
#define FONT_LAST_CHAR    0x7EU   /* ASCII ตัวสุดท้ายที่ตารางรองรับ (~) */
#define FONT_WIDTH        5U      /* แต่ละตัวอักษรกว้าง 5 คอลัมน์ (วาดจริงจะเว้น +1 คอลัมน์ระหว่างตัว) */
#define FONT_HEIGHT       7U      /* สูง 7 แถว (เก็บเป็น 1 Byte ต่อคอลัมน์ ใช้ 7 บิตล่าง, บิตบนสุดไม่ใช้) */

/* ตารางฟอนต์ 5x7 มาตรฐาน (ASCII 0x20-0x7E) แบบ Column-major: แต่ละตัวอักษรมี 5 Byte (5 คอลัมน์)
 * ในแต่ละ Byte บิต 0 = แถวบนสุด, บิต 6 = แถวล่างสุด (บิต 7 ไม่ใช้)
 */
extern const uint8_t font5x7_table[(uint32_t)FONT_LAST_CHAR - (uint32_t)FONT_FIRST_CHAR + 1U][FONT_WIDTH];

#endif /* FONT5X7_H_ */
