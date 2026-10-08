#ifndef FONT5X7_H_
#define FONT5X7_H_

#include <stdint.h>

#define FONT_FIRST_CHAR   0x20U   /* ASCII ตัวแรกที่ตารางรองรับ (space) */
#define FONT_LAST_CHAR    0x7EU   /* ASCII ตัวสุดท้ายที่ตารางรองรับ (~) */
#define FONT_WIDTH        5U      /* แต่ละตัวอักษรกว้าง 5 คอลัมน์ (วาดจริงจะเว้น +1 คอลัมน์ระหว่างตัว) */
#define FONT_HEIGHT       7U      /* สูง 7 แถว (เก็บเป็น 1 Byte ต่อคอลัมน์ ใช้ 7 บิตล่าง, บิตบนสุดไม่ใช้) */

#define FONT_CHAR_COUNT   (((uint32_t)FONT_LAST_CHAR - (uint32_t)FONT_FIRST_CHAR) + 1U)   /* 95 ตัวอักษร */
#define FONT_TABLE_SIZE   (FONT_CHAR_COUNT * FONT_WIDTH)

/* ตารางฟอนต์ 5x7 มาตรฐาน (ASCII 0x20-0x7E) เก็บเรียงต่อกันเป็น Array มิติเดียว ตัวอักษรละ 5 Byte (5 คอลัมน์)
 * ตำแหน่งเริ่มของตัวอักษร c = (c - FONT_FIRST_CHAR) * FONT_WIDTH
 * ในแต่ละ Byte บิต 0 = แถวบนสุด, บิต 6 = แถวล่างสุด (บิต 7 ไม่ใช้)
 */
extern const uint8_t font5x7_table[FONT_TABLE_SIZE];

#endif /* FONT5X7_H_ */
