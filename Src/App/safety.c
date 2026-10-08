#include "App/safety.h"
#include "Drivers/uart_driver.h"
#include "Drivers/gpio_driver.h"

/* Lockout ถูกเก็บเป็น Static เพราะ safety.c เป็นเจ้าของ State นี้แต่เพียงผู้เดียว
 * (fsm.c อ่านผ่าน Safety_IsLockout() เท่านั้น ไม่แก้ไขค่าเอง)
 */
#define PIN_LOW     0U
#define PIN_HIGH    1U

static uint8_t lockout_active = 0U;

/* เก็บค่า Sensor ล่าสุดไว้ให้โมดูลอื่นอ่านผ่าน Getter ด้านล่าง (เช่น display.c เอาไปโชว์หน้าจอ OLED) */
static float last_temp_c = 0.0f;
static uint8_t last_humidity_pct = 0U;
static uint8_t has_reading = 0U;

void Safety_Init(void) {
    lockout_active = 0U;

    /* ตั้งค่าขา PC2 เป็น Digital Output สำหรับ Relay พัดลมระบายอากาศ
     * เริ่มต้นที่ LOW (พัดลมหยุด) จนกว่าอุณหภูมิหรือความชื้นจะเกินเกณฑ์จริงผ่าน Safety_Update()
     */
    GPIO_SetPinMode(TEMP_ALARM_OUT_PORT, TEMP_ALARM_OUT_PIN, GPIO_MODE_OUTPUT);
    GPIO_WritePin(TEMP_ALARM_OUT_PORT, TEMP_ALARM_OUT_PIN, PIN_LOW);

    /* ตั้งต้นเป็นสถานะปกติไปก่อน จนกว่าจะมีค่า Sensor จริงเข้ามาผ่าน Safety_Update() ครั้งแรก */
    LED_On(LED1_PORT, LED1_PIN);
    LED_Off(LED2_PORT, LED2_PIN);
    LED_Off(LED3_PORT, LED3_PIN);
}

void Safety_Update(float temp_c, uint8_t humidity_pct) {
    uint8_t temp_bad;
    uint8_t humid_bad;
    uint8_t abnormal;

    if (temp_c > SAFETY_TEMP_MAX_C) {
        temp_bad = 1U;
    } else {
        temp_bad = 0U;
    }

    if (humidity_pct > SAFETY_HUMID_MAX_PCT) {
        humid_bad = 1U;
    } else {
        humid_bad = 0U;
    }

    if ((temp_bad != 0U) || (humid_bad != 0U)) {
        abnormal = 1U;
    } else {
        abnormal = 0U;
    }

    /* เก็บค่าล่าสุดไว้ก่อนเลย ให้ Getter เรียกอ่านได้เสมอไม่ว่าจะเกินเกณฑ์หรือไม่ */
    last_temp_c = temp_c;
    last_humidity_pct = humidity_pct;
    has_reading = 1U;

    /* 1. ควบคุม LED1 (Normal) / LED2 (Temp Alarm) / LED3 (Humid Alarm) และขา PC2 (Relay พัดลม)
     * (ไม่มีการพิมพ์สถานะออก UART ตอนปกติ — เงียบสนิท ต่อเมื่อเกินเกณฑ์เท่านั้นถึงจะเห็นข้อความ ดูข้อ 2)
     * PC2 ผูกกับ abnormal (Temp หรือ Humid เกิน) ที่จุดเดียว เพื่อไม่ให้บล็อกหนึ่งสั่งทับอีกบล็อก
     */
    if (abnormal != 0U) {
        LED_Off(LED1_PORT, LED1_PIN);
        GPIO_WritePin(TEMP_ALARM_OUT_PORT, TEMP_ALARM_OUT_PIN, PIN_HIGH);
    } else {
        LED_On(LED1_PORT, LED1_PIN);
        GPIO_WritePin(TEMP_ALARM_OUT_PORT, TEMP_ALARM_OUT_PIN, PIN_LOW);
    }

    /* LED2 = อุณหภูมิเกิน, LED3 = ความชื้นเกิน (บอกสาเหตุของการ Lockout) */
    if (temp_bad != 0U) {
        LED_On(LED2_PORT, LED2_PIN);
    } else {
        LED_Off(LED2_PORT, LED2_PIN);
    }

    if (humid_bad != 0U) {
        LED_On(LED3_PORT, LED3_PIN);
    } else {
        LED_Off(LED3_PORT, LED3_PIN);
    }

    /* 2. ข้อความแจ้งเตือนวนซ้ำทุกครั้งที่ Update (~ทุก 2 วินาที) ตราบใดที่ยังเกินเกณฑ์อยู่
     *    แสดงค่าอุณหภูมิ/ความชื้นปัจจุบันกำกับไปด้วยทุกครั้ง ตามที่ต้องการ (Loop จนกว่าจะกลับปกติ)
     */
    if ((temp_bad != 0U) && (humid_bad != 0U)) {
        UART2_SendString("[WARNING] BOTH Temp & Humidity Exceeded! (Temp: ");
        UART2_SendFloat1(temp_c);
        UART2_SendString(" C, Humid: ");
        UART2_SendUint(humidity_pct);
        UART2_SendString(" %)\r\n");
    } else if (temp_bad != 0U) {
        UART2_SendString("[WARNING] Temp Exceeded! (Current Temp: ");
        UART2_SendFloat1(temp_c);
        UART2_SendString(" C | Limit: ");
        UART2_SendFloat1(SAFETY_TEMP_MAX_C);   /* ดึงจากค่าเกณฑ์จริง ถ้าแก้เกณฑ์ ข้อความก็เปลี่ยนตาม */
        UART2_SendString(" C)\r\n");
    } else if (humid_bad != 0U) {
        UART2_SendString("[WARNING] Humidity Exceeded! (Current Humid: ");
        UART2_SendUint(humidity_pct);
        UART2_SendString(" % | Limit: ");
        UART2_SendUint(SAFETY_HUMID_MAX_PCT);
        UART2_SendString(" %)\r\n");
    } else {
        /* ปกติ ไม่ต้องพิมพ์ข้อความเตือนซ้ำ */
    }

    /* 3. ตรวจจับการเปลี่ยนสถานะ Lock/Unlock (Edge) เพื่อพิมพ์ข้อความแค่ครั้งเดียวตอนเปลี่ยนสถานะ
     *    (ไม่วนซ้ำ — ต่างจากข้อ 2 ที่วนซ้ำทุกรอบขณะยังผิดปกติ)
     */
    if ((abnormal != 0U) && (lockout_active == 0U)) {
        lockout_active = 1U;
        UART2_SendString("[SYSTEM] Environment Abnormal - System Locked\r\n");
    } else if ((abnormal == 0U) && (lockout_active != 0U)) {
        lockout_active = 0U;
        UART2_SendString("[SYSTEM] Environment Restored - System Ready\r\n");
    } else {
        /* สถานะเดิมไม่เปลี่ยน ไม่ต้องพิมพ์ซ้ำ */
    }
}

uint8_t Safety_IsLockout(void) {
    return lockout_active;
}

float Safety_GetLastTemp(void) {
    return last_temp_c;
}

uint8_t Safety_GetLastHumidity(void) {
    return last_humidity_pct;
}

uint8_t Safety_HasReading(void) {
    return has_reading;
}\

