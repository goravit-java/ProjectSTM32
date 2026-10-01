#ifndef MENU_H_
#define MENU_H_

#include <stdint.h>

#define MENU_ITEM_COUNT   4U
#define MENU_NAME_MAXLEN  24U /* ต้องเผื่อชื่อยาวสุดคือ "Japanese Cucumber" (18 ตัวอักษร + null) */

/* โครงสร้างข้อมูลของสินค้าแต่ละรายการ (ชื่อ, ราคา, จำนวน Stock) */
typedef struct {
    char    name[MENU_NAME_MAXLEN];
    uint16_t price; /* หน่วยเป็นบาท (จำนวนเต็ม) */
    uint8_t stock;
} MenuItem_t;

extern MenuItem_t menu[MENU_ITEM_COUNT];

void Menu_Init(void);
void Menu_PrintAll(void);

#endif /* MENU_H_ */
