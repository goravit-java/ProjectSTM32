#include "App/menu.h"
#include "Drivers/uart_driver.h"
#include <string.h>

MenuItem_t menu[MENU_ITEM_COUNT];

/* รายการสินค้า ราคา (บาท) และ Stock เริ่มต้น ตามเอกสาร "Project ฉบับปรับปรุง"
 * (เปลี่ยนโดเมนจากตู้เครื่องดื่ม เป็นตู้จำหน่ายผักและผลไม้อัจฉริยะ)
 */
#define ITEM_SALAD              0U
#define ITEM_CUCUMBER           1U
#define ITEM_APPLE              2U
#define ITEM_GRAPE              3U

#define SALAD_PRICE_THB         25U
#define SALAD_INIT_STOCK        3U
#define CUCUMBER_PRICE_THB      15U
#define CUCUMBER_INIT_STOCK     2U
#define APPLE_PRICE_THB         20U
#define APPLE_INIT_STOCK        1U
#define GRAPE_PRICE_THB         30U
#define GRAPE_INIT_STOCK        0U      /* สินค้าหมด ตามสเปก */

#define NAME_COPY_LEN           (MENU_NAME_MAXLEN - 1U)   /* เว้นที่ให้ตัวปิดท้าย '\0' เสมอ */

void Menu_Init(void) {
    (void)strncpy(menu[ITEM_SALAD].name, "Fresh Salad", NAME_COPY_LEN);
    menu[ITEM_SALAD].price = SALAD_PRICE_THB;
    menu[ITEM_SALAD].stock = SALAD_INIT_STOCK;

    (void)strncpy(menu[ITEM_CUCUMBER].name, "Japanese Cucumber", NAME_COPY_LEN);
    menu[ITEM_CUCUMBER].price = CUCUMBER_PRICE_THB;
    menu[ITEM_CUCUMBER].stock = CUCUMBER_INIT_STOCK;

    (void)strncpy(menu[ITEM_APPLE].name, "Red Apple", NAME_COPY_LEN);
    menu[ITEM_APPLE].price = APPLE_PRICE_THB;
    menu[ITEM_APPLE].stock = APPLE_INIT_STOCK;

    (void)strncpy(menu[ITEM_GRAPE].name, "Organic Grape", NAME_COPY_LEN);
    menu[ITEM_GRAPE].price = GRAPE_PRICE_THB;
    menu[ITEM_GRAPE].stock = GRAPE_INIT_STOCK;
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
