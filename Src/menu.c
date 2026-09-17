#include "menu.h"
#include "uart_driver.h"
#include <string.h>

MenuItem_t menu[MENU_ITEM_COUNT];

/* หมายเหตุ: เอกสารภาพรวมโครงงานระบุเฉพาะชื่อเมนูและ Stock เริ่มต้นเท่านั้น
 * ราคาด้านล่างเป็นค่าตัวอย่าง (สมมติ) สามารถแก้ไขให้ตรงกับราคาจริงที่ต้องการได้
 */
void Menu_Init(void) {
    (void)strncpy(menu[0].name, "Salad", MENU_NAME_MAXLEN - 1U);
    menu[0].price = 15U;
    menu[0].stock = 3U;

    (void)strncpy(menu[1].name, "Cabbage", MENU_NAME_MAXLEN - 1U);
    menu[1].price = 15U;
    menu[1].stock = 2U;

    (void)strncpy(menu[2].name, "Melon", MENU_NAME_MAXLEN - 1U);
    menu[2].price = 20U;
    menu[2].stock = 1U;

    (void)strncpy(menu[3].name, "Grape", MENU_NAME_MAXLEN - 1U);
    menu[3].price = 20U;
    menu[3].stock = 0U;
}

void Menu_PrintAll(void) {
    uint8_t i;

    UART2_SendString("\r\n----------- MENU -----------\r\n");
    for (i = 0U; i < MENU_ITEM_COUNT; i++) {
        UART2_SendString(menu[i].name);
        UART2_SendString(" | Price: ");
        UART2_SendUint(menu[i].price);
        UART2_SendString(" Baht | Stock: ");
        UART2_SendUint(menu[i].stock);
        UART2_SendString("\r\n");
    }
    UART2_SendString("-----------------------------\r\n");
}
