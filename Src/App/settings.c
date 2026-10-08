#include "App/settings.h"
#include "App/safety.h"

#define KNOB_FULL_SCALE        4095U   /* ADC 12-bit */
#define KNOB_ROUND_HALF        2U      /* ตัวหารครึ่งหนึ่ง ใช้ปัดเศษให้ใกล้ขั้นที่สุด */
#define KNOB_TAKEOVER_COUNTS   120U    /* ต้องหมุนออกจากตำแหน่งเดิมเกิน ~3% ค่าถึงจะเริ่มเปลี่ยนตามปุ่มหมุน */
#define KNOB_HYSTERESIS        20U     /* ต้องหมุนเลยรอยต่อขั้นไปอีก 20 count ถึงจะเปลี่ยนขั้น (กันตัวเลขแกว่ง) */
#define HUMID_STEPS            (SAFETY_HUMID_LIMIT_MAX_PCT - SAFETY_HUMID_LIMIT_MIN_PCT)   /* 50 ขั้น ทีละ 1 % */

static uint8_t editing = 0U;
static uint8_t field = SETTINGS_FIELD_TEMP;
static uint8_t pending_temp_step = SAFETY_TEMP_LIMIT_DEFAULT;
static uint8_t pending_humid_step = (uint8_t)(SAFETY_HUMID_LIMIT_DEFAULT - SAFETY_HUMID_LIMIT_MIN_PCT);
static uint8_t knob_active = 0U;     /* 1 = หมุนแล้ว ค่าของหัวข้อนี้เปลี่ยนตามปุ่มหมุน */
static uint8_t activity = 0U;
static uint16_t knob_last = 0U;      /* ค่าปุ่มหมุนล่าสุด (อัปเดตเสมอ แม้ไม่ได้อยู่หน้า SETTINGS) */
static uint16_t knob_ref = 0U;       /* ตำแหน่งปุ่มหมุนตอนเริ่มแก้หัวข้อนี้ */

static uint8_t Settings_Quantize(uint16_t raw, uint32_t steps, uint8_t current, uint8_t use_hysteresis);
static uint16_t Settings_AbsDiff(uint16_t a, uint16_t b);

void Settings_Enter(void) {
    editing = 1U;
    field = SETTINGS_FIELD_TEMP;
    pending_temp_step = Safety_GetTempLimitStep();
    pending_humid_step = (uint8_t)(Safety_GetHumidLimit() - SAFETY_HUMID_LIMIT_MIN_PCT);
    knob_active = 0U;
    knob_ref = knob_last;
    activity = 0U;
}

void Settings_Exit(void) {
    editing = 0U;
    knob_active = 0U;
}

void Settings_Save(void) {
    Safety_SetLimits(pending_temp_step, (uint8_t)(pending_humid_step + SAFETY_HUMID_LIMIT_MIN_PCT));
    Settings_Exit();
}

void Settings_NextField(void) {
    if (field == SETTINGS_FIELD_TEMP) {
        field = SETTINGS_FIELD_HUMID;
    } else {
        field = SETTINGS_FIELD_TEMP;
    }
    /* หัวข้อใหม่ต้องหมุนก่อนค่าถึงจะเริ่มตาม (ไม่ให้ค่ากระโดดไปตามตำแหน่งที่ปุ่มหมุนค้างไว้) */
    knob_active = 0U;
    knob_ref = knob_last;
}

void Settings_OnKnob(uint16_t knob_raw) {
    uint8_t use_hysteresis = 1U;   /* ครั้งแรกที่เริ่มหมุน: กระโดดไปตามตำแหน่งปุ่มหมุนทันที ไม่ใช้ Hysteresis */
    uint8_t new_step;

    knob_last = knob_raw;

    if (editing == 0U) {
        /* ไม่ได้อยู่หน้า SETTINGS: ปุ่มหมุนไม่มีผล */
    } else {
        if ((knob_active == 0U) && (Settings_AbsDiff(knob_raw, knob_ref) > KNOB_TAKEOVER_COUNTS)) {
            knob_active = 1U;
            use_hysteresis = 0U;
        } else {
            /* ยังไม่หมุน หรือเริ่มตามปุ่มหมุนไปแล้ว */
        }

        if (knob_active != 0U) {
            if (field == SETTINGS_FIELD_TEMP) {
                new_step = Settings_Quantize(knob_raw, SAFETY_TEMP_LIMIT_STEPS, pending_temp_step, use_hysteresis);
                if (new_step != pending_temp_step) {
                    pending_temp_step = new_step;
                    activity = 1U;
                } else {
                    /* ค่าเท่าเดิม */
                }
            } else {
                new_step = Settings_Quantize(knob_raw, HUMID_STEPS, pending_humid_step, use_hysteresis);
                if (new_step != pending_humid_step) {
                    pending_humid_step = new_step;
                    activity = 1U;
                } else {
                    /* ค่าเท่าเดิม */
                }
            }
        } else {
            /* ยังไม่เริ่มตามปุ่มหมุน */
        }
    }
}

uint8_t Settings_TakeActivity(void) {
    uint8_t result = activity;

    activity = 0U;
    return result;
}

uint8_t Settings_GetField(void) {
    return field;
}

uint8_t Settings_GetPendingTempStep(void) {
    return pending_temp_step;
}

uint8_t Settings_GetPendingHumid(void) {
    return (uint8_t)(pending_humid_step + SAFETY_HUMID_LIMIT_MIN_PCT);
}

/* แปลงค่าปุ่มหมุน (0-4095) เป็นขั้น 0..steps (ปัดเศษ)
 * use_hysteresis = 1: เปลี่ยนขั้นเฉพาะเมื่อห่างจากกึ่งกลางขั้นปัจจุบันเกินครึ่งขั้น + KNOB_HYSTERESIS
 */
static uint8_t Settings_Quantize(uint16_t raw, uint32_t steps, uint8_t current, uint8_t use_hysteresis) {
    uint32_t value = (uint32_t)raw;
    uint32_t candidate;
    uint32_t center;
    uint32_t distance;
    uint32_t half_step = KNOB_FULL_SCALE / (KNOB_ROUND_HALF * steps);
    uint8_t result = current;

    if (value > KNOB_FULL_SCALE) {
        value = KNOB_FULL_SCALE;
    } else {
        /* อยู่ในช่วง 12-bit ปกติ */
    }

    candidate = ((value * steps) + (KNOB_FULL_SCALE / KNOB_ROUND_HALF)) / KNOB_FULL_SCALE;
    center = ((uint32_t)current * KNOB_FULL_SCALE) / steps;

    if (value > center) {
        distance = value - center;
    } else {
        distance = center - value;
    }

    if (candidate == (uint32_t)current) {
        /* ยังอยู่ขั้นเดิม */
    } else if ((use_hysteresis == 0U) || (distance > (half_step + KNOB_HYSTERESIS))) {
        result = (uint8_t)candidate;
    } else {
        /* อยู่ในช่วง Hysteresis ใกล้รอยต่อ: คงขั้นเดิม */
    }
    return result;
}

static uint16_t Settings_AbsDiff(uint16_t a, uint16_t b) {
    uint16_t diff;

    if (a > b) {
        diff = (uint16_t)(a - b);
    } else {
        diff = (uint16_t)(b - a);
    }
    return diff;
}
