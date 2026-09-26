#include "menu.h"
#include "uart_driver.h"
#include <string.h>

MenuItem_t menu[MENU_ITEM_COUNT];

/* รายการสินค้า ราคา และ Stock เริ่มต้น ตามเอกสาร "Project ฉบับปรับปรุง"
 * (เปลี่ยนโดเมนจากตู้เครื่องดื่ม เป็นตู้จำหน่ายผักและผลไม้อัจฉริยะ)
 */
void Menu_Init(void) {
    (void)strncpy(menu[0].name, "Fresh Salad", MENU_NAME_MAXLEN - 1U);
    menu[0].price = 25U;
    menu[0].stock = 3U;

    (void)strncpy(menu[1].name, "Japanese Cucumber", MENU_NAME_MAXLEN - 1U);
    menu[1].price = 15U;
    menu[1].stock = 2U;

    (void)strncpy(menu[2].name, "Red Apple", MENU_NAME_MAXLEN - 1U);
    menu[2].price = 20U;
    menu[2].stock = 1U;

    (void)strncpy(menu[3].name, "Organic Grape", MENU_NAME_MAXLEN - 1U);
    menu[3].price = 30U;
    menu[3].stock = 0U; /* สินค้าหมด ตามสเปก */
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
