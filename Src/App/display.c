#include "App/display.h"
#include "Drivers/ssd1306_driver.h"
#include "App/fsm.h"
#include "App/safety.h"
#include "App/menu.h"

/* หน่วงความถี่การ Refresh จอ OLED ไว้ที่ ~ทุก 8 รอบ Main Loop (Loop หลักวิ่งทุก ~20ms ดังนั้นประมาณ 160ms/ครั้ง)
 * เพราะการส่งข้อมูลเต็มจอ (1024 Byte) ผ่าน I2C ใช้เวลาระดับหลัก ms ต่อครั้ง ถ้าทำทุก Loop จะหน่วงปุ่มกด/FSM
 */
#define DISPLAY_REFRESH_TICKS   8U

static uint32_t disp_tick = 0U;

static void Display_DrawIdleScreen(void);
static void Display_DrawMenuScreen(void);
static void Display_DrawConfirmScreen(void);
static void Display_DrawDispensingScreen(void);
static uint16_t Display_TextWidth(const char *str);
static void Display_DrawLockoutScreen(void);

void Display_Init(void) {
    SSD1306_Init();
    disp_tick = 0U;
}

void Display_Update(void) {
    disp_tick++;
    if (disp_tick < DISPLAY_REFRESH_TICKS) {
        return; /* ยังไม่ครบรอบ Refresh -> ไม่แตะ I2C เลย ประหยัดเวลาของ Main Loop */
    }
    disp_tick = 0U;

    SSD1306_Clear();

    /* Safety Lockout มีสิทธิ์สูงสุด: บังคับตัดมาหน้า Lockout ทันทีไม่ว่า FSM จะอยู่ State ไหนอยู่ก็ตาม */
    if (Safety_IsLockout() != 0U) {
        Display_DrawLockoutScreen();
    } else {
        SystemState_t state = FSM_GetState();

        if ((state == STATE_PROCESSING) || (state == STATE_COMPLETE)) {
            Display_DrawDispensingScreen();
        } else if ((state == STATE_IDLE) || (state == STATE_INIT) || (state == STATE_FAULT)) {
            Display_DrawIdleScreen();
        } else if ((state == STATE_CONFIRM) || (state == STATE_CHECK_STOCK) || (state == STATE_SAFETY_CHECK)) {
            /* CHECK_STOCK/SAFETY_CHECK ผ่านไปภายใน 1 Tick หลังกด OK จึงค้างหน้า Confirm ไว้ให้ภาพต่อเนื่อง */
            Display_DrawConfirmScreen();
        } else {
            Display_DrawMenuScreen();
        }
    }

    SSD1306_UpdateScreen();
}

/* Screen 0: IDLE (หน้าพักรอลูกค้า) แสดงชื่อตู้ วิธีเริ่มใช้งาน และค่าสภาวะแวดล้อมปัจจุบันที่ Footer
 * ข้อความทุกบรรทัดจัดกึ่งกลางจอ: x = (128 - จำนวนตัวอักษร x 6px) / 2
 */
static void Display_DrawIdleScreen(void) {
    uint16_t x;

    (void)SSD1306_DrawString(7U, 0U, "== SMART VENDING ==");   /* 19 ตัวอักษร = 114px */
    SSD1306_DrawHLine(0U, 10U, (uint16_t)SSD1306_WIDTH);

    (void)SSD1306_DrawString(13U, 22U, "[ PRESS OK / UP ]");    /* 17 ตัวอักษร = 102px */
    (void)SSD1306_DrawString(19U, 32U, "To Select Drink");      /* 15 ตัวอักษร = 90px */

    SSD1306_DrawHLine(0U, 50U, (uint16_t)SSD1306_WIDTH);

    /* Footer: ก่อนอ่าน Sensor ครั้งแรก (~2 วินาทีหลังเปิดเครื่อง) ยังไม่มีค่าจริง จึงแสดง "--" แทน 0 */
    x = SSD1306_DrawString(0U, 55U, "Temp:");
    if (Safety_HasReading() != 0U) {
        x = SSD1306_DrawFloat1(x, 55U, Safety_GetLastTemp());
    } else {
        x = SSD1306_DrawString(x, 55U, "--.-");
    }
    (void)SSD1306_DrawString(x, 55U, "C");

    x = SSD1306_DrawString(68U, 55U, "Hum:");
    if (Safety_HasReading() != 0U) {
        x = SSD1306_DrawFloat1(x, 55U, (float)Safety_GetLastHumidity());
    } else {
        x = SSD1306_DrawString(x, 55U, "--.-");
    }
    (void)SSD1306_DrawString(x, 55U, "%");
}

/* Screen 1: SELECT_DRINK / CONFIRM / CHECK_STOCK / SAFETY_CHECK (เลือกสินค้า สภาวะปกติ ยังไม่ Lockout) */
static void Display_DrawMenuScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    uint16_t x;

    (void)SSD1306_DrawString(0U, 0U, "VEGGIE & FRUIT");

    x = SSD1306_DrawString(96U, 0U, "[");
    x = SSD1306_DrawUint(x, 0U, (uint32_t)(idx + 1U));
    x = SSD1306_DrawString(x, 0U, "/");
    x = SSD1306_DrawUint(x, 0U, (uint32_t)MENU_ITEM_COUNT);
    (void)SSD1306_DrawString(x, 0U, "]");

    x = SSD1306_DrawString(0U, 16U, "> ");
    (void)SSD1306_DrawString(x, 16U, menu[idx].name);

    if (menu[idx].stock == 0U) {
        (void)SSD1306_DrawString(6U, 32U, "*OUT OF STOCK*");
    } else {
        x = SSD1306_DrawString(6U, 32U, "Price: ");
        x = SSD1306_DrawUint(x, 32U, (uint32_t)menu[idx].price);
        (void)SSD1306_DrawString(x, 32U, " THB");

        x = SSD1306_DrawString(6U, 42U, "Stock: ");
        x = SSD1306_DrawUint(x, 42U, (uint32_t)menu[idx].stock);
        (void)SSD1306_DrawString(x, 42U, " pcs");
    }

    SSD1306_DrawHLine(0U, 52U, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(0U, 56U, "UP/DN Move  OK Select");
}

/* Screen 1.5: CONFIRM ORDER (ยืนยันการสั่งซื้อ) แสดงชื่อและราคาสินค้าที่เลือก รอผู้ใช้กด OK หรือ BACK
 * จอกว้างได้ 21 ตัวอักษร: ชื่อยาวสุด "Japanese Cucumber" (17 ตัว) ใส่ ">> ... <<" ไม่พอ
 * จึงเลือกกรอบตามความยาวชื่อ แล้วจัดกึ่งกลางจอเสมอ
 */
static void Display_DrawConfirmScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    const char *name = menu[idx].name;
    uint16_t name_w = Display_TextWidth(name);
    uint16_t x;

    (void)SSD1306_DrawString(0U, 0U, "CONFIRM ORDER");
    SSD1306_DrawHLine(0U, 10U, (uint16_t)SSD1306_WIDTH);

    if (name_w <= 90U) {            /* ชื่อไม่เกิน 15 ตัวอักษร: ">> name <<" */
        x = (uint16_t)(((uint16_t)SSD1306_WIDTH - (name_w + 36U)) / 2U);
        x = SSD1306_DrawString(x, 20U, ">> ");
        x = SSD1306_DrawString(x, 20U, name);
        (void)SSD1306_DrawString(x, 20U, " <<");
    } else if (name_w <= 102U) {    /* 16-17 ตัวอักษร: "> name <" */
        x = (uint16_t)(((uint16_t)SSD1306_WIDTH - (name_w + 24U)) / 2U);
        x = SSD1306_DrawString(x, 20U, "> ");
        x = SSD1306_DrawString(x, 20U, name);
        (void)SSD1306_DrawString(x, 20U, " <");
    } else {                        /* ยาวกว่านั้น: แสดงชื่ออย่างเดียว */
        (void)SSD1306_DrawString(0U, 20U, name);
    }

    x = SSD1306_DrawString(28U, 34U, "Price: ");
    x = SSD1306_DrawUint(x, 34U, (uint32_t)menu[idx].price);
    (void)SSD1306_DrawString(x, 34U, " THB");

    SSD1306_DrawHLine(0U, 50U, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(0U, 55U, "[OK]Yes");          /* 7 ตัวอักษร = 42px ชิดซ้าย */
    (void)SSD1306_DrawString(56U, 55U, "[BACK]Cancel");    /* 12 ตัวอักษร = 72px ชิดขวา (56 + 72 = 128) */
}

/* ความกว้างข้อความเป็นพิกเซล (ตัวละ 6px) นับไม่เกิน MENU_NAME_MAXLEN กันอ่านเลยขอบ Array */
static uint16_t Display_TextWidth(const char *str) {
    uint16_t count = 0U;

    while ((count < (uint16_t)MENU_NAME_MAXLEN) && (str[count] != '\0')) {
        count++;
    }
    return (uint16_t)(count * 6U);
}

/* Screen 2: PROCESSING / COMPLETE (กำลังจ่ายสินค้า) */
static void Display_DrawDispensingScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    uint32_t percent = FSM_GetProgressPercent();
    uint32_t seconds_left = FSM_GetSecondsLeft();
    uint16_t x;
    uint16_t fill_width;

    (void)SSD1306_DrawString(0U, 0U, "-- DISPENSING --");

    x = SSD1306_DrawString(0U, 16U, "Item: ");
    (void)SSD1306_DrawString(x, 16U, menu[idx].name);

    /* Progress Bar: กรอบที่ x=4..124 (กว้าง 120px), y=28..37 (สูง 10px) */
    SSD1306_DrawRect(4U, 28U, 120U, 10U);
    fill_width = (uint16_t)((118U * percent) / 100U); /* เว้นขอบด้านละ 1px ไม่ให้ชนกรอบ */
    if (fill_width > 0U) {
        SSD1306_FillRect(5U, 29U, fill_width, 8U);
    }

    x = SSD1306_DrawUint(0U, 44U, percent);
    (void)SSD1306_DrawString(x, 44U, "%");

    if (FSM_GetState() == STATE_COMPLETE) {
        (void)SSD1306_DrawString(0U, 56U, "Done! Enjoy your pick!");
    } else {
        x = SSD1306_DrawString(0U, 56U, "Processing... (");
        x = SSD1306_DrawUint(x, 56U, seconds_left);
        (void)SSD1306_DrawString(x, 56U, "s)");
    }
}

/* Screen 3: SAFETY LOCKOUT ALARM (อุณหภูมิ และ/หรือ ความชื้น เกินเกณฑ์) */
static void Display_DrawLockoutScreen(void) {
    float temp = Safety_GetLastTemp();
    uint8_t humid = Safety_GetLastHumidity();
    uint8_t temp_bad = (temp > SAFETY_TEMP_MAX_C) ? 1U : 0U;
    uint8_t humid_bad = (humid > SAFETY_HUMID_MAX_PCT) ? 1U : 0U;
    uint16_t x;

    (void)SSD1306_DrawString(0U, 0U, "!! SYSTEM LOCKOUT !!");

    if ((temp_bad != 0U) && (humid_bad != 0U)) {
        x = SSD1306_DrawString(0U, 16U, "Temp: ");
        x = SSD1306_DrawFloat1(x, 16U, temp);
        (void)SSD1306_DrawString(x, 16U, "C [OVER]");

        x = SSD1306_DrawString(0U, 26U, "Hum:  ");
        x = SSD1306_DrawUint(x, 26U, (uint32_t)humid);
        (void)SSD1306_DrawString(x, 26U, "% [OVER]");

        SSD1306_DrawHLine(0U, 50U, (uint16_t)SSD1306_WIDTH);
        (void)SSD1306_DrawString(0U, 56U, "BOTH EXCEEDED!");
    } else if (temp_bad != 0U) {
        (void)SSD1306_DrawString(0U, 16U, "TEMP OVERHEAT!");

        x = SSD1306_DrawString(0U, 28U, "Current: ");
        x = SSD1306_DrawFloat1(x, 28U, temp);
        (void)SSD1306_DrawString(x, 28U, "C");

        x = SSD1306_DrawString(0U, 38U, "Limit: ");
        x = SSD1306_DrawFloat1(x, 38U, SAFETY_TEMP_MAX_C);
        (void)SSD1306_DrawString(x, 38U, "C");

        SSD1306_DrawHLine(0U, 50U, (uint16_t)SSD1306_WIDTH);
        (void)SSD1306_DrawString(0U, 56U, "Cooling Down...");
    } else if (humid_bad != 0U) {
        (void)SSD1306_DrawString(0U, 16U, "HUMIDITY OVER!");

        x = SSD1306_DrawString(0U, 28U, "Current: ");
        x = SSD1306_DrawUint(x, 28U, (uint32_t)humid);
        (void)SSD1306_DrawString(x, 28U, "%");

        x = SSD1306_DrawString(0U, 38U, "Limit: ");
        x = SSD1306_DrawUint(x, 38U, (uint32_t)SAFETY_HUMID_MAX_PCT);
        (void)SSD1306_DrawString(x, 38U, "%");

        SSD1306_DrawHLine(0U, 50U, (uint16_t)SSD1306_WIDTH);
        (void)SSD1306_DrawString(0U, 56U, "Waiting Normal...");
    } else {
        /* กรณีนี้ไม่ควรเกิดจริง เพราะเข้ามาเฉพาะตอน Safety_IsLockout()=1 เท่านั้น แต่กันพลาดไว้ (Defensive) */
        (void)SSD1306_DrawString(0U, 16U, "Checking...");
    }
}
