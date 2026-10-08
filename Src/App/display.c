#include "App/display.h"
#include "Drivers/ssd1306_driver.h"
#include "App/fsm.h"
#include "App/safety.h"
#include "App/menu.h"
#include "App/settings.h"

/* หน่วงความถี่การ Refresh จอ OLED ไว้ที่ ~ทุก 8 รอบ Main Loop (Loop หลักวิ่งทุก ~20ms ดังนั้นประมาณ 160ms/ครั้ง)
 * เพราะการส่งข้อมูลเต็มจอ (1024 Byte) ผ่าน I2C ใช้เวลาระดับหลัก ms ต่อครั้ง ถ้าทำทุก Loop จะหน่วงปุ่มกด/FSM
 */
#define DISPLAY_REFRESH_TICKS   8U

/* ---------------- ตำแหน่งบนจอ 128 x 64 (หน่วย Pixel) ---------------- */
/* ใช้ร่วมกันทุกหน้า */
#define X_LEFT                  0U    /* ชิดขอบซ้าย */
#define Y_HEADER                0U    /* แถวหัวข้อบนสุด */
#define Y_HEADER_LINE           10U   /* เส้นคั่นใต้หัวข้อ */
#define Y_FOOTER_LINE           50U   /* เส้นคั่นเหนือ Footer */
#define Y_FOOTER_TEXT           55U   /* ข้อความ Footer ใต้เส้นคั่นที่ Y_FOOTER_LINE */
#define Y_BOTTOM_TEXT           56U   /* ข้อความแถวล่างสุด (หน้าเมนู/จ่ายสินค้า/Lockout) */
#define CENTER_DIVISOR          2U    /* (กว้างจอ - กว้างข้อความ) / 2 = จัดกึ่งกลาง */

/* หน้า IDLE: ข้อความจัดกึ่งกลาง x = (128 - จำนวนตัวอักษร x 6) / 2 */
#define IDLE_TITLE_X            7U    /* "== SMART VENDING ==" 19 ตัวอักษร = 114 px */
#define IDLE_PROMPT_X           13U   /* "[ PRESS OK / UP ]"   17 ตัวอักษร = 102 px */
#define IDLE_PROMPT_Y           22U
#define IDLE_HINT_X             19U   /* "To Select Drink"     15 ตัวอักษร = 90 px */
#define IDLE_HINT_Y             32U
#define IDLE_HUMID_X            68U   /* ครึ่งขวาของ Footer */
#define IDLE_LIMIT_X            13U   /* "Limit:40.0C / 70%"   17 ตัวอักษร = 102 px */
#define IDLE_LIMIT_Y            41U   /* ระหว่างข้อความ Hint (y=32) กับเส้น Footer (y=50) */

/* หน้าเลือกสินค้า */
#define MENU_COUNTER_X          96U   /* "[1/4]" ชิดขวาบน */
#define MENU_ITEM_Y             16U
#define MENU_INDENT_X           6U
#define MENU_PRICE_Y            32U
#define MENU_STOCK_Y            42U
#define MENU_FOOTER_LINE_Y      52U

/* หน้า CONFIRM ORDER */
#define CONFIRM_NAME_Y              20U
#define CONFIRM_PRICE_X             28U
#define CONFIRM_PRICE_Y             34U
#define CONFIRM_BACK_X              56U   /* "[BACK]Cancel" 12 ตัวอักษร = 72 px ชิดขวา (56 + 72 = 128) */
#define CONFIRM_WIDE_MAX_CHARS      15U   /* ชื่อไม่เกิน 15 ตัว ใส่กรอบ ">> name <<" ได้ */
#define CONFIRM_NARROW_MAX_CHARS    17U   /* ชื่อ 16-17 ตัว ใช้กรอบ "> name <" */
#define CONFIRM_WIDE_FRAME_CHARS    6U    /* ">> " + " <<" */
#define CONFIRM_NARROW_FRAME_CHARS  4U    /* "> " + " <" */

/* หน้า DISPENSING: Progress Bar กรอบที่ x=4..123 (กว้าง 120 px), y=28..37 (สูง 10 px) */
#define DISP_ITEM_Y             16U
#define BAR_X                   4U
#define BAR_Y                   28U
#define BAR_WIDTH               120U
#define BAR_HEIGHT              10U
#define BAR_FILL_X              5U    /* เว้นขอบด้านละ 1 px ไม่ให้ชนกรอบ */
#define BAR_FILL_Y              29U
#define BAR_FILL_MAX_WIDTH      118U
#define BAR_FILL_HEIGHT         8U
#define PERCENT_FULL            100U
#define DISP_PERCENT_Y          44U

/* หน้า PAYMENT และ PAYMENT FAILED */
#define PAY_TITLE_X             1U    /* "PAYMENT (10THB/BLOCK)" 21 ตัวอักษร = 126 px */
#define PAY_NAME_Y              16U
#define PAY_PRICE_Y             26U
#define PAY_PAID_Y              36U
#define PAY_NEED_LABEL_CHARS    5U    /* "NEED " */
#define PAY_TIME_LABEL_CHARS    5U    /* "Time:" */
#define PAY_TIME_SUFFIX_CHARS   1U    /* "s" */
#define FAIL_TITLE_X            1U    /* "!! PAYMENT TIMEOUT !!" 21 ตัวอักษร = 126 px */
#define FAIL_MSG1_X             22U   /* "PAYMENT FAILED"       14 ตัวอักษร = 84 px */
#define FAIL_MSG1_Y             22U
#define FAIL_MSG2_X             4U    /* "Returning to Menu..." 20 ตัวอักษร = 120 px */
#define FAIL_MSG2_Y             32U
#define FAIL_FOOTER_X           1U    /* "[FAIL] Money Returned" 21 ตัวอักษร = 126 px */
#define DECIMAL_BASE            10U

/* หน้า LOCKOUT */
#define LOCK_ROW1_Y             16U
#define LOCK_ROW2_BOTH_Y        26U   /* กรณีเกินทั้งคู่: แถวที่ 2 ชิดขึ้นมาเพื่อแสดง 2 ค่า */
#define LOCK_ROW2_Y             28U
#define LOCK_ROW3_Y             38U

/* หน้า SETTINGS */
#define SET_TITLE_X             22U   /* "== SETTINGS ==" 14 ตัวอักษร = 84 px */
#define SET_TITLE_LOCK_X        10U   /* "SETTINGS  !LOCKED!" 18 ตัวอักษร = 108 px */
#define SET_TEMP_Y              16U
#define SET_HUMID_Y             28U
#define SET_HINT_Y              40U

static uint32_t disp_tick = 0U;

static void Display_DrawIdleScreen(void);
static void Display_DrawMenuScreen(void);
static void Display_DrawConfirmScreen(void);
static void Display_DrawDispensingScreen(void);
static void Display_DrawPaymentScreen(void);
static void Display_DrawPaymentFailedScreen(void);
static uint16_t Display_UintWidth(uint32_t value);
static uint16_t Display_TextWidth(const char *str);
static void Display_DrawLockoutFooter(const char *status_text);
static void Display_DrawLockoutScreen(void);
static void Display_DrawSettingsScreen(void);

void Display_Init(void) {
    SSD1306_Init();
    disp_tick = 0U;
}

void Display_Update(void) {
    disp_tick++;

    if (disp_tick >= DISPLAY_REFRESH_TICKS) {
        disp_tick = 0U;
        SSD1306_Clear();

        /* หน้า SETTINGS แสดงก่อนหน้า Lockout (ผู้ดูแลต้องเห็นค่าที่กำลังตั้ง หัวจอมีป้าย !LOCKED! บอกแทน)
         * นอกนั้น Safety Lockout มีสิทธิ์สูงสุด: บังคับตัดมาหน้า Lockout ทันทีไม่ว่า FSM จะอยู่ State ไหน
         */
        if (FSM_GetState() == STATE_SETTINGS) {
            Display_DrawSettingsScreen();
        } else if (Safety_IsLockout() != 0U) {
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
            } else if (state == STATE_PAYMENT) {
                Display_DrawPaymentScreen();
            } else if (state == STATE_PAYMENT_FAILED) {
                Display_DrawPaymentFailedScreen();
            } else {
                Display_DrawMenuScreen();
            }
        }

        SSD1306_UpdateScreen();
    } else {
        /* ยังไม่ครบรอบ Refresh -> ไม่แตะ I2C เลย ประหยัดเวลาของ Main Loop */
    }
}

/* Screen 0: IDLE (หน้าพักรอลูกค้า) แสดงชื่อตู้ วิธีเริ่มใช้งาน และค่าสภาวะแวดล้อมปัจจุบันที่ Footer */
static void Display_DrawIdleScreen(void) {
    uint16_t x;

    (void)SSD1306_DrawString(IDLE_TITLE_X, Y_HEADER, "== SMART VENDING ==");
    SSD1306_DrawHLine(X_LEFT, Y_HEADER_LINE, (uint16_t)SSD1306_WIDTH);

    (void)SSD1306_DrawString(IDLE_PROMPT_X, IDLE_PROMPT_Y, "[ PRESS OK / UP ]");
    (void)SSD1306_DrawString(IDLE_HINT_X, IDLE_HINT_Y, "To Select Drink");

    /* เกณฑ์ปัจจุบัน (ตั้งได้ที่หน้า SETTINGS: กด BACK ค้าง) */
    x = SSD1306_DrawString(IDLE_LIMIT_X, IDLE_LIMIT_Y, "Limit:");
    x = SSD1306_DrawFloat1(x, IDLE_LIMIT_Y, Safety_GetTempLimit());
    x = SSD1306_DrawString(x, IDLE_LIMIT_Y, "C / ");
    x = SSD1306_DrawUint(x, IDLE_LIMIT_Y, (uint32_t)Safety_GetHumidLimit());
    (void)SSD1306_DrawString(x, IDLE_LIMIT_Y, "%");

    SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);

    /* Footer: ก่อนอ่าน Sensor ครั้งแรก (~2 วินาทีหลังเปิดเครื่อง) ยังไม่มีค่าจริง จึงแสดง "--" แทน 0 */
    x = SSD1306_DrawString(X_LEFT, Y_FOOTER_TEXT, "Temp:");
    if (Safety_HasReading() != 0U) {
        x = SSD1306_DrawFloat1(x, Y_FOOTER_TEXT, Safety_GetLastTemp());
    } else {
        x = SSD1306_DrawString(x, Y_FOOTER_TEXT, "--.-");
    }
    (void)SSD1306_DrawString(x, Y_FOOTER_TEXT, "C");

    x = SSD1306_DrawString(IDLE_HUMID_X, Y_FOOTER_TEXT, "Hum:");
    if (Safety_HasReading() != 0U) {
        x = SSD1306_DrawFloat1(x, Y_FOOTER_TEXT, (float)Safety_GetLastHumidity());
    } else {
        x = SSD1306_DrawString(x, Y_FOOTER_TEXT, "--.-");
    }
    (void)SSD1306_DrawString(x, Y_FOOTER_TEXT, "%");
}

/* Screen 1: SELECT_DRINK (เลือกสินค้า สภาวะปกติ ยังไม่ Lockout) */
static void Display_DrawMenuScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    uint16_t x;

    (void)SSD1306_DrawString(X_LEFT, Y_HEADER, "VEGGIE & FRUIT");

    x = SSD1306_DrawString(MENU_COUNTER_X, Y_HEADER, "[");
    x = SSD1306_DrawUint(x, Y_HEADER, (uint32_t)idx + 1U);
    x = SSD1306_DrawString(x, Y_HEADER, "/");
    x = SSD1306_DrawUint(x, Y_HEADER, (uint32_t)MENU_ITEM_COUNT);
    (void)SSD1306_DrawString(x, Y_HEADER, "]");

    x = SSD1306_DrawString(X_LEFT, MENU_ITEM_Y, "> ");
    (void)SSD1306_DrawString(x, MENU_ITEM_Y, menu[idx].name);

    if (menu[idx].stock == 0U) {
        (void)SSD1306_DrawString(MENU_INDENT_X, MENU_PRICE_Y, "*OUT OF STOCK*");
    } else {
        x = SSD1306_DrawString(MENU_INDENT_X, MENU_PRICE_Y, "Price: ");
        x = SSD1306_DrawUint(x, MENU_PRICE_Y, (uint32_t)menu[idx].price);
        (void)SSD1306_DrawString(x, MENU_PRICE_Y, " THB");

        x = SSD1306_DrawString(MENU_INDENT_X, MENU_STOCK_Y, "Stock: ");
        x = SSD1306_DrawUint(x, MENU_STOCK_Y, (uint32_t)menu[idx].stock);
        (void)SSD1306_DrawString(x, MENU_STOCK_Y, " pcs");
    }

    SSD1306_DrawHLine(X_LEFT, MENU_FOOTER_LINE_Y, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(X_LEFT, Y_BOTTOM_TEXT, "UP/DN Move  OK Select");
}

/* Screen 1.5: CONFIRM ORDER (ยืนยันการสั่งซื้อ) แสดงชื่อและราคาสินค้าที่เลือก รอผู้ใช้กด OK หรือ BACK
 * จอกว้างได้ 21 ตัวอักษร: ชื่อยาวสุด "Japanese Cucumber" (17 ตัว) ใส่ ">> ... <<" ไม่พอ
 * จึงเลือกกรอบตามความยาวชื่อ แล้วจัดกึ่งกลางจอเสมอ
 */
static void Display_DrawConfirmScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    const char *name = menu[idx].name;
    uint16_t name_w = Display_TextWidth(name);
    uint16_t frame_w;
    uint16_t x;

    (void)SSD1306_DrawString(X_LEFT, Y_HEADER, "CONFIRM ORDER");
    SSD1306_DrawHLine(X_LEFT, Y_HEADER_LINE, (uint16_t)SSD1306_WIDTH);

    if (name_w <= (CONFIRM_WIDE_MAX_CHARS * SSD1306_CHAR_ADVANCE)) {
        frame_w = (uint16_t)(name_w + (CONFIRM_WIDE_FRAME_CHARS * SSD1306_CHAR_ADVANCE));
        x = (uint16_t)(((uint16_t)SSD1306_WIDTH - frame_w) / CENTER_DIVISOR);
        x = SSD1306_DrawString(x, CONFIRM_NAME_Y, ">> ");
        x = SSD1306_DrawString(x, CONFIRM_NAME_Y, name);
        (void)SSD1306_DrawString(x, CONFIRM_NAME_Y, " <<");
    } else if (name_w <= (CONFIRM_NARROW_MAX_CHARS * SSD1306_CHAR_ADVANCE)) {
        frame_w = (uint16_t)(name_w + (CONFIRM_NARROW_FRAME_CHARS * SSD1306_CHAR_ADVANCE));
        x = (uint16_t)(((uint16_t)SSD1306_WIDTH - frame_w) / CENTER_DIVISOR);
        x = SSD1306_DrawString(x, CONFIRM_NAME_Y, "> ");
        x = SSD1306_DrawString(x, CONFIRM_NAME_Y, name);
        (void)SSD1306_DrawString(x, CONFIRM_NAME_Y, " <");
    } else {
        /* ชื่อยาวกว่านั้น: แสดงชื่ออย่างเดียว */
        (void)SSD1306_DrawString(X_LEFT, CONFIRM_NAME_Y, name);
    }

    x = SSD1306_DrawString(CONFIRM_PRICE_X, CONFIRM_PRICE_Y, "Price: ");
    x = SSD1306_DrawUint(x, CONFIRM_PRICE_Y, (uint32_t)menu[idx].price);
    (void)SSD1306_DrawString(x, CONFIRM_PRICE_Y, " THB");

    SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(X_LEFT, Y_FOOTER_TEXT, "[OK]Yes");
    (void)SSD1306_DrawString(CONFIRM_BACK_X, Y_FOOTER_TEXT, "[BACK]Cancel");
}

/* ความกว้างข้อความเป็นพิกเซล นับไม่เกิน MENU_NAME_MAXLEN กันอ่านเลยขอบ Array */
static uint16_t Display_TextWidth(const char *str) {
    uint16_t count = 0U;

    while ((count < (uint16_t)MENU_NAME_MAXLEN) && (str[count] != '\0')) {
        count++;
    }
    return (uint16_t)(count * SSD1306_CHAR_ADVANCE);
}

/* Screen 2.1: PAYMENT (รับชำระเงินผ่านเซ็นเซอร์แสง)
 * แสดงชื่อสินค้า ราคา ยอดที่จ่ายแล้ว ยอดที่ยังขาด และเวลาที่เหลือ
 * ตัวเลขด้านขวา (NEED / Time) จัดชิดขวาจอตามจำนวนหลักจริง
 */
static void Display_DrawPaymentScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    uint32_t price = (uint32_t)menu[idx].price;
    uint32_t paid = FSM_GetPaidAmount();
    uint32_t seconds_left = FSM_GetSecondsLeft();
    uint32_t need;
    uint16_t right_w;
    uint16_t x;

    if (paid < price) {
        need = price - paid;
    } else {
        need = 0U;
    }

    (void)SSD1306_DrawString(PAY_TITLE_X, Y_HEADER, "PAYMENT (10THB/BLOCK)");
    SSD1306_DrawHLine(X_LEFT, Y_HEADER_LINE, (uint16_t)SSD1306_WIDTH);

    (void)SSD1306_DrawString(X_LEFT, PAY_NAME_Y, menu[idx].name);

    x = SSD1306_DrawString(X_LEFT, PAY_PRICE_Y, "Price: ");
    x = SSD1306_DrawUint(x, PAY_PRICE_Y, price);
    (void)SSD1306_DrawString(x, PAY_PRICE_Y, " THB");

    x = SSD1306_DrawString(X_LEFT, PAY_PAID_Y, "Paid:  ");
    x = SSD1306_DrawUint(x, PAY_PAID_Y, paid);
    (void)SSD1306_DrawString(x, PAY_PAID_Y, " THB");

    right_w = (uint16_t)((PAY_NEED_LABEL_CHARS * SSD1306_CHAR_ADVANCE) + Display_UintWidth(need));
    x = SSD1306_DrawString((uint16_t)((uint16_t)SSD1306_WIDTH - right_w), PAY_PAID_Y, "NEED ");
    (void)SSD1306_DrawUint(x, PAY_PAID_Y, need);

    SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(X_LEFT, Y_FOOTER_TEXT, "Cover Sensor");

    right_w = (uint16_t)(((PAY_TIME_LABEL_CHARS + PAY_TIME_SUFFIX_CHARS) * SSD1306_CHAR_ADVANCE)
                         + Display_UintWidth(seconds_left));
    x = SSD1306_DrawString((uint16_t)((uint16_t)SSD1306_WIDTH - right_w), Y_FOOTER_TEXT, "Time:");
    x = SSD1306_DrawUint(x, Y_FOOTER_TEXT, seconds_left);
    (void)SSD1306_DrawString(x, Y_FOOTER_TEXT, "s");
}

/* Screen 2.2: PAYMENT FAILED (หมดเวลาแต่ยอดเงินไม่ครบ) แสดง 3 วินาทีแล้ว FSM พากลับหน้าแรกเอง */
static void Display_DrawPaymentFailedScreen(void) {
    (void)SSD1306_DrawString(FAIL_TITLE_X, Y_HEADER, "!! PAYMENT TIMEOUT !!");
    SSD1306_DrawHLine(X_LEFT, Y_HEADER_LINE, (uint16_t)SSD1306_WIDTH);

    (void)SSD1306_DrawString(FAIL_MSG1_X, FAIL_MSG1_Y, "PAYMENT FAILED");
    (void)SSD1306_DrawString(FAIL_MSG2_X, FAIL_MSG2_Y, "Returning to Menu...");

    SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(FAIL_FOOTER_X, Y_FOOTER_TEXT, "[FAIL] Money Returned");
}

/* ความกว้าง (pixel) ของตัวเลขจำนวนเต็มเมื่อวาดด้วยฟอนต์ 5x7 ใช้จัดข้อความชิดขวา */
static uint16_t Display_UintWidth(uint32_t value) {
    uint16_t digits = 1U;
    uint32_t v = value;

    while (v >= DECIMAL_BASE) {
        v /= DECIMAL_BASE;
        digits++;
    }
    return (uint16_t)(digits * SSD1306_CHAR_ADVANCE);
}

/* Screen 2: PROCESSING / COMPLETE (กำลังจ่ายสินค้า) */
static void Display_DrawDispensingScreen(void) {
    uint8_t idx = FSM_GetSelectedIndex();
    uint32_t percent = FSM_GetProgressPercent();
    uint32_t seconds_left = FSM_GetSecondsLeft();
    uint16_t x;
    uint16_t fill_width;

    (void)SSD1306_DrawString(X_LEFT, Y_HEADER, "-- DISPENSING --");

    x = SSD1306_DrawString(X_LEFT, DISP_ITEM_Y, "Item: ");
    (void)SSD1306_DrawString(x, DISP_ITEM_Y, menu[idx].name);

    SSD1306_DrawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT);
    fill_width = (uint16_t)((BAR_FILL_MAX_WIDTH * percent) / PERCENT_FULL);
    if (fill_width > 0U) {
        SSD1306_FillRect(BAR_FILL_X, BAR_FILL_Y, fill_width, BAR_FILL_HEIGHT);
    } else {
        /* 0%: แสดงแค่กรอบเปล่า */
    }

    x = SSD1306_DrawUint(X_LEFT, DISP_PERCENT_Y, percent);
    (void)SSD1306_DrawString(x, DISP_PERCENT_Y, "%");

    if (FSM_GetState() == STATE_COMPLETE) {
        (void)SSD1306_DrawString(X_LEFT, Y_BOTTOM_TEXT, "Done! Enjoy your pick!");
    } else {
        x = SSD1306_DrawString(X_LEFT, Y_BOTTOM_TEXT, "Processing... (");
        x = SSD1306_DrawUint(x, Y_BOTTOM_TEXT, seconds_left);
        (void)SSD1306_DrawString(x, Y_BOTTOM_TEXT, "s)");
    }
}

/* Screen 3: SAFETY LOCKOUT ALARM (อุณหภูมิ และ/หรือ ความชื้น เกินเกณฑ์) */
static void Display_DrawLockoutScreen(void) {
    float temp = Safety_GetLastTemp();
    uint8_t humid = Safety_GetLastHumidity();
    uint8_t temp_bad;
    uint8_t humid_bad;
    uint16_t x;

    if (temp > Safety_GetTempLimit()) {
        temp_bad = 1U;
    } else {
        temp_bad = 0U;
    }

    if (humid > Safety_GetHumidLimit()) {
        humid_bad = 1U;
    } else {
        humid_bad = 0U;
    }

    (void)SSD1306_DrawString(X_LEFT, Y_HEADER, "!! SYSTEM LOCKOUT !!");

    if ((temp_bad != 0U) && (humid_bad != 0U)) {
        x = SSD1306_DrawString(X_LEFT, LOCK_ROW1_Y, "Temp: ");
        x = SSD1306_DrawFloat1(x, LOCK_ROW1_Y, temp);
        (void)SSD1306_DrawString(x, LOCK_ROW1_Y, "C [OVER]");

        x = SSD1306_DrawString(X_LEFT, LOCK_ROW2_BOTH_Y, "Hum:  ");
        x = SSD1306_DrawUint(x, LOCK_ROW2_BOTH_Y, (uint32_t)humid);
        (void)SSD1306_DrawString(x, LOCK_ROW2_BOTH_Y, "% [OVER]");

        SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
        Display_DrawLockoutFooter("BOTH EXCEEDED!");
    } else if (temp_bad != 0U) {
        (void)SSD1306_DrawString(X_LEFT, LOCK_ROW1_Y, "TEMP OVERHEAT!");

        x = SSD1306_DrawString(X_LEFT, LOCK_ROW2_Y, "Current: ");
        x = SSD1306_DrawFloat1(x, LOCK_ROW2_Y, temp);
        (void)SSD1306_DrawString(x, LOCK_ROW2_Y, "C");

        x = SSD1306_DrawString(X_LEFT, LOCK_ROW3_Y, "Limit: ");
        x = SSD1306_DrawFloat1(x, LOCK_ROW3_Y, Safety_GetTempLimit());
        (void)SSD1306_DrawString(x, LOCK_ROW3_Y, "C");

        SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
        Display_DrawLockoutFooter("Cooling Down...");
    } else if (humid_bad != 0U) {
        (void)SSD1306_DrawString(X_LEFT, LOCK_ROW1_Y, "HUMIDITY OVER!");

        x = SSD1306_DrawString(X_LEFT, LOCK_ROW2_Y, "Current: ");
        x = SSD1306_DrawUint(x, LOCK_ROW2_Y, (uint32_t)humid);
        (void)SSD1306_DrawString(x, LOCK_ROW2_Y, "%");

        x = SSD1306_DrawString(X_LEFT, LOCK_ROW3_Y, "Limit: ");
        x = SSD1306_DrawUint(x, LOCK_ROW3_Y, (uint32_t)Safety_GetHumidLimit());
        (void)SSD1306_DrawString(x, LOCK_ROW3_Y, "%");

        SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
        Display_DrawLockoutFooter("Waiting Normal...");
    } else {
        /* กรณีนี้ไม่ควรเกิดจริง เพราะเข้ามาเฉพาะตอน Safety_IsLockout()=1 เท่านั้น แต่กันพลาดไว้ (Defensive) */
        (void)SSD1306_DrawString(X_LEFT, LOCK_ROW1_Y, "Checking...");
    }
}

/* Footer ของหน้า Lockout: ถ้ารายการซื้อเพิ่งถูกยกเลิกเพราะล็อกกลางคัน ให้แจ้งลูกค้าแทนข้อความสถานะปกติ */
static void Display_DrawLockoutFooter(const char *status_text) {
    if (FSM_IsOrderCancelled() != 0U) {
        (void)SSD1306_DrawString(X_LEFT, Y_BOTTOM_TEXT, "ORDER CANCELLED!");
    } else {
        (void)SSD1306_DrawString(X_LEFT, Y_BOTTOM_TEXT, status_text);
    }
}

/* Screen: SETTINGS (ตั้งเกณฑ์ด้วยปุ่มหมุน)
 * แถวละหัวข้อ: "> Temp : 35.5C (40.0)" = ค่าที่กำลังตั้ง และ (ค่าเดิมที่ใช้อยู่) ลูกศรชี้หัวข้อที่เลือก
 */
static void Display_DrawSettingsScreen(void) {
    uint16_t x;

    if (Safety_IsLockout() != 0U) {
        (void)SSD1306_DrawString(SET_TITLE_LOCK_X, Y_HEADER, "SETTINGS  !LOCKED!");
    } else {
        (void)SSD1306_DrawString(SET_TITLE_X, Y_HEADER, "== SETTINGS ==");
    }
    SSD1306_DrawHLine(X_LEFT, Y_HEADER_LINE, (uint16_t)SSD1306_WIDTH);

    if (Settings_GetField() == SETTINGS_FIELD_TEMP) {
        x = SSD1306_DrawString(X_LEFT, SET_TEMP_Y, "> Temp : ");
    } else {
        x = SSD1306_DrawString(X_LEFT, SET_TEMP_Y, "  Temp : ");
    }
    x = SSD1306_DrawFloat1(x, SET_TEMP_Y, Safety_TempStepToC(Settings_GetPendingTempStep()));
    x = SSD1306_DrawString(x, SET_TEMP_Y, "C (");
    x = SSD1306_DrawFloat1(x, SET_TEMP_Y, Safety_GetTempLimit());
    (void)SSD1306_DrawString(x, SET_TEMP_Y, ")");

    if (Settings_GetField() == SETTINGS_FIELD_HUMID) {
        x = SSD1306_DrawString(X_LEFT, SET_HUMID_Y, "> Humid: ");
    } else {
        x = SSD1306_DrawString(X_LEFT, SET_HUMID_Y, "  Humid: ");
    }
    x = SSD1306_DrawUint(x, SET_HUMID_Y, (uint32_t)Settings_GetPendingHumid());
    x = SSD1306_DrawString(x, SET_HUMID_Y, "%  (");
    x = SSD1306_DrawUint(x, SET_HUMID_Y, (uint32_t)Safety_GetHumidLimit());
    (void)SSD1306_DrawString(x, SET_HUMID_Y, ")");

    (void)SSD1306_DrawString(X_LEFT, SET_HINT_Y, "Turn knob, UP/DN sel");

    SSD1306_DrawHLine(X_LEFT, Y_FOOTER_LINE, (uint16_t)SSD1306_WIDTH);
    (void)SSD1306_DrawString(X_LEFT, Y_FOOTER_TEXT, "[OK]Save [BACK]Cancel");
}
