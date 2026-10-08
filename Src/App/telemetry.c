#include "App/telemetry.h"
#include "App/fsm.h"
#include "App/safety.h"
#include "App/menu.h"
#include "App/settings.h"
#include "Drivers/uart_driver.h"

#define TELEMETRY_MIN_GAP_TICKS     10U    /* ส่งถี่สุด 1 ครั้งต่อ 10 Tick = 200 ms (ไม่ให้ท่วม UART) */
#define TELEMETRY_HEARTBEAT_TICKS   250U   /* ไม่มีอะไรเปลี่ยนก็ส่งซ้ำทุก 250 Tick = 5 วินาที */
#define TEMP_SCALE_X10              (10.0f) /* เทียบอุณหภูมิเป็นจำนวนเต็มหน่วย 0.1°C (ห้ามเทียบ float ด้วย ==) */

/* ภาพรวมสถานะ ณ ขณะหนึ่ง ใช้เทียบกับครั้งก่อนว่ามีอะไรเปลี่ยนหรือไม่ */
typedef struct {
    SystemState_t state;
    int32_t temp_x10;
    int32_t limit_x10;
    uint8_t humid_limit;
    uint8_t set_field;
    uint8_t set_temp_step;
    uint8_t set_humid;
    uint8_t humidity;
    uint8_t has_reading;
    uint8_t lock;
    uint8_t item;
    uint32_t paid;
    uint32_t seconds_left;
    uint32_t progress;
    uint8_t cancelled;
    uint8_t stock[MENU_ITEM_COUNT];
} TelemetrySnapshot_t;

static TelemetrySnapshot_t last_sent;
static uint8_t has_sent = 0U;
static uint32_t ticks_since_send = 0U;

static void Telemetry_Capture(TelemetrySnapshot_t *snap);
static uint8_t Telemetry_IsSame(const TelemetrySnapshot_t *a, const TelemetrySnapshot_t *b);
static void Telemetry_Send(const TelemetrySnapshot_t *snap);
static const char *Telemetry_StateName(SystemState_t state);

void Telemetry_Update(void) {
    TelemetrySnapshot_t now;
    uint8_t should_send = 0U;

    if (ticks_since_send < TELEMETRY_HEARTBEAT_TICKS) {
        ticks_since_send++;
    } else {
        /* ถึงเวลา Heartbeat แล้ว: หยุดนับไว้ที่ค่านี้ */
    }

    Telemetry_Capture(&now);

    if (has_sent == 0U) {
        should_send = 1U;
    } else if (ticks_since_send >= TELEMETRY_HEARTBEAT_TICKS) {
        should_send = 1U;
    } else if ((ticks_since_send >= TELEMETRY_MIN_GAP_TICKS) && (Telemetry_IsSame(&now, &last_sent) == 0U)) {
        should_send = 1U;
    } else {
        /* ไม่มีอะไรเปลี่ยน หรือเพิ่งส่งไปไม่ถึง 200 ms: รอรอบถัดไป */
    }

    if (should_send != 0U) {
        Telemetry_Send(&now);
        last_sent = now;
        has_sent = 1U;
        ticks_since_send = 0U;
    } else {
        /* No action */
    }
}

static void Telemetry_Capture(TelemetrySnapshot_t *snap) {
    uint8_t i;

    snap->state = FSM_GetState();
    snap->temp_x10 = (int32_t)(Safety_GetLastTemp() * TEMP_SCALE_X10);
    snap->limit_x10 = (int32_t)(Safety_GetTempLimit() * TEMP_SCALE_X10);
    snap->humid_limit = Safety_GetHumidLimit();
    snap->set_field = Settings_GetField();
    snap->set_temp_step = Settings_GetPendingTempStep();
    snap->set_humid = Settings_GetPendingHumid();
    snap->humidity = Safety_GetLastHumidity();
    snap->has_reading = Safety_HasReading();
    snap->lock = Safety_IsLockout();
    snap->item = FSM_GetSelectedIndex();
    snap->paid = FSM_GetPaidAmount();
    snap->seconds_left = FSM_GetSecondsLeft();
    snap->progress = FSM_GetProgressPercent();
    snap->cancelled = FSM_IsOrderCancelled();
    for (i = 0U; i < MENU_ITEM_COUNT; i++) {
        snap->stock[i] = menu[i].stock;
    }
}

static uint8_t Telemetry_IsSame(const TelemetrySnapshot_t *a, const TelemetrySnapshot_t *b) {
    uint8_t same = 1U;
    uint8_t i;

    if ((a->state != b->state) || (a->temp_x10 != b->temp_x10) || (a->humidity != b->humidity)) {
        same = 0U;
    } else if ((a->limit_x10 != b->limit_x10) || (a->humid_limit != b->humid_limit)) {
        same = 0U;
    } else if ((a->set_field != b->set_field) || (a->set_temp_step != b->set_temp_step) || (a->set_humid != b->set_humid)) {
        same = 0U;
    } else if ((a->has_reading != b->has_reading) || (a->lock != b->lock) || (a->item != b->item)) {
        same = 0U;
    } else if ((a->paid != b->paid) || (a->seconds_left != b->seconds_left) || (a->progress != b->progress)) {
        same = 0U;
    } else if (a->cancelled != b->cancelled) {
        same = 0U;
    } else {
        for (i = 0U; i < MENU_ITEM_COUNT; i++) {
            if (a->stock[i] != b->stock[i]) {
                same = 0U;
            } else {
                /* สต็อกรายการนี้เท่าเดิม */
            }
        }
    }
    return same;
}

static void Telemetry_Send(const TelemetrySnapshot_t *snap) {
    uint8_t i;

    UART2_SendString("#S state=");
    UART2_SendString(Telemetry_StateName(snap->state));
    UART2_SendString(" temp=");
    UART2_SendFloat1(Safety_GetLastTemp());
    UART2_SendString(" tlim=");
    UART2_SendFloat1(Safety_GetTempLimit());
    UART2_SendString(" hlim=");
    UART2_SendUint((uint32_t)snap->humid_limit);
    UART2_SendString(" hum=");
    UART2_SendUint((uint32_t)snap->humidity);
    UART2_SendString(" sens=");
    UART2_SendUint((uint32_t)snap->has_reading);
    UART2_SendString(" lock=");
    UART2_SendUint((uint32_t)snap->lock);
    UART2_SendString(" item=");
    UART2_SendUint((uint32_t)snap->item);
    UART2_SendString(" price=");
    UART2_SendUint((uint32_t)menu[snap->item].price);
    UART2_SendString(" paid=");
    UART2_SendUint(snap->paid);
    UART2_SendString(" left=");
    UART2_SendUint(snap->seconds_left);
    UART2_SendString(" prog=");
    UART2_SendUint(snap->progress);
    UART2_SendString(" cancel=");
    UART2_SendUint((uint32_t)snap->cancelled);
    UART2_SendString(" sf=");
    UART2_SendUint((uint32_t)snap->set_field);
    UART2_SendString(" spt=");
    UART2_SendFloat1(Safety_TempStepToC(snap->set_temp_step));
    UART2_SendString(" sph=");
    UART2_SendUint((uint32_t)snap->set_humid);
    UART2_SendString(" stock=");
    for (i = 0U; i < MENU_ITEM_COUNT; i++) {
        if (i > 0U) {
            UART2_SendChar(',');
        } else {
            /* รายการแรก: ไม่มีตัวคั่นนำหน้า */
        }
        UART2_SendUint((uint32_t)snap->stock[i]);
    }
    UART2_SendString("\r\n");
}

static const char *Telemetry_StateName(SystemState_t state) {
    const char *name;

    switch (state) {
    case STATE_INIT:
        name = "INIT";
        break;
    case STATE_IDLE:
        name = "IDLE";
        break;
    case STATE_SELECT_DRINK:
        name = "SELECT";
        break;
    case STATE_CONFIRM:
        name = "CONFIRM";
        break;
    case STATE_CHECK_STOCK:
        name = "CHECK_STOCK";
        break;
    case STATE_PAYMENT:
        name = "PAYMENT";
        break;
    case STATE_PAYMENT_FAILED:
        name = "PAYMENT_FAILED";
        break;
    case STATE_SAFETY_CHECK:
        name = "SAFETY_CHECK";
        break;
    case STATE_PROCESSING:
        name = "PROCESSING";
        break;
    case STATE_COMPLETE:
        name = "COMPLETE";
        break;
    case STATE_SETTINGS:
        name = "SETTINGS";
        break;
    case STATE_FAULT:
        name = "FAULT";
        break;
    default:
        name = "UNKNOWN";
        break;
    }
    return name;
}
