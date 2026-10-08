#include "App/fsm.h"
#include "Drivers/gpio_driver.h"
#include "Drivers/uart_driver.h"
#include "App/safety.h"
#include "App/menu.h"
#include "Drivers/light_sensor_driver.h"
#include "App/settings.h"

/* Main loop เรียก FSM_Run() ทุก ๆ 20ms (คุมจังหวะด้วย TIM2 ใน main.c ดู MAIN_LOOP_PERIOD_US)
 * ค่า TICKS ด้านล่างจึงอิงจากรอบเวลานั้น ใช้แทนการ Block ด้วย Delay ตรง ๆ ใน State
 * เพื่อไม่ให้ IWDG_Refresh() และการรับปุ่มค้างระหว่างรอ
 */
#define TICKS_PER_SECOND     50U /* 1000ms / 20ms ต่อรอบ Loop */
#define PROCESSING_SECONDS    4U /* Dispense Simulation: นับถอยหลัง 4 วินาที (อยู่ในช่วง 3-5 วิ ตามสเปก) */
#define PROCESSING_TICKS     (PROCESSING_SECONDS * TICKS_PER_SECOND)
#define COMPLETE_TICKS        (1U * TICKS_PER_SECOND) /* ~1 วินาที : เวลาแสดงผล "เสร็จสิ้น" ก่อนกลับ IDLE */
#define LED4_BLINK_TICKS      25U /* Toggle LED4 ทุก 25 Tick = 0.5 วินาที ระหว่างจ่ายสินค้า */
#define PERCENT_FULL          100U

/* ระบบชำระเงินด้วยเซ็นเซอร์แสง */
#define COIN_VALUE_THB          10U   /* บังแสง 1 ครั้ง = 10 บาท */
#define PAYMENT_TIMEOUT_SECONDS 30U
#define PAYMENT_TIMEOUT_TICKS   (PAYMENT_TIMEOUT_SECONDS * TICKS_PER_SECOND)
#define PAYMENT_FAILED_TICKS    (3U * TICKS_PER_SECOND)   /* แสดงหน้า "ชำระเงินไม่สำเร็จ" 3 วินาที */

/* หน้า SETTINGS: ไม่มีการกดปุ่มหรือหมุนปุ่มนาน 20 วินาที -> ออกเองโดยไม่บันทึก */
#define SETTINGS_TIMEOUT_TICKS  (20U * TICKS_PER_SECOND)

static SystemState_t current_state = STATE_INIT;
static uint8_t selected_index = 0U;
static uint32_t state_tick = 0U;

/* 1 = รายการซื้อล่าสุดถูกยกเลิกเพราะระบบล็อกกลางคัน (ให้ display.c แจ้งลูกค้าบนหน้า Lockout)
 * เคลียร์เป็น 0 อัตโนมัติเมื่อกลับเข้า IDLE (ดู FSM_EnterState)
 */
static uint8_t order_cancelled = 0U;

/* ยอดเงินสะสมของรายการปัจจุบัน (บาท) รีเซ็ตเป็น 0 ทุกครั้งที่เข้า PAYMENT และเมื่อกลับ IDLE */
static uint32_t paid_amount = 0U;

static void FSM_UpdateOutputs(void);
static void FSM_EnterState(SystemState_t new_state);
static void FSM_CancelForLockout(const char *stage_text);
static void FSM_PrintRefund(void);
static void FSM_OpenSettings(void);

void FSM_Init(void) {
    current_state = STATE_INIT;
    selected_index = 0U;
    state_tick = 0U;
}

SystemState_t FSM_GetState(void) {
    return current_state;
}

uint8_t FSM_IsOrderCancelled(void) {
    return order_cancelled;
}

uint32_t FSM_GetPaidAmount(void) {
    return paid_amount;
}

uint8_t FSM_GetSelectedIndex(void) {
    return selected_index;
}

/* คำนวณเปอร์เซ็นต์ความคืบหน้าจาก state_tick ปัจจุบันเทียบกับ PROCESSING_TICKS ทั้งหมด
 * (ใช้ Logic เดียวกับที่ fsm.c ใช้ตัดสินใจเปลี่ยน State เอง เพื่อไม่ให้ค่าที่โชว์ไม่ตรงกับความเป็นจริง)
 */
uint32_t FSM_GetProgressPercent(void) {
    uint32_t percent;

    if (current_state == STATE_COMPLETE) {
        percent = PERCENT_FULL;
    } else if (current_state == STATE_PROCESSING) {
        percent = (state_tick * PERCENT_FULL) / PROCESSING_TICKS;
        if (percent > PERCENT_FULL) {
            percent = PERCENT_FULL; /* กันไว้เผื่อรอบ Tick สุดท้ายก่อนเปลี่ยน State (Defensive) */
        } else {
            /* อยู่ในช่วง 0-100 อยู่แล้ว */
        }
    } else {
        percent = 0U;
    }
    return percent;
}

uint32_t FSM_GetSecondsLeft(void) {
    uint32_t seconds_elapsed;
    uint32_t seconds_left = 0U;

    if (current_state == STATE_PROCESSING) {
        seconds_elapsed = state_tick / TICKS_PER_SECOND;
        if (seconds_elapsed < (uint32_t)PROCESSING_SECONDS) {
            seconds_left = (uint32_t)PROCESSING_SECONDS - seconds_elapsed;
        } else {
            /* ครบเวลาแล้ว: เหลือ 0 วินาที */
        }
    } else if (current_state == STATE_PAYMENT) {
        if (state_tick < PAYMENT_TIMEOUT_TICKS) {
            /* ปัดขึ้น: จอแสดง 30, 29, ... 1 วินาที (ไม่โชว์ 0 ทั้งที่ยังเหลือเวลาไม่ถึง 1 วินาที) */
            seconds_left = ((PAYMENT_TIMEOUT_TICKS - state_tick) + (TICKS_PER_SECOND - 1U)) / TICKS_PER_SECOND;
        } else {
            /* หมดเวลาแล้ว: เหลือ 0 วินาที */
        }
    } else {
        /* ไม่ได้อยู่ในช่วงนับถอยหลัง: เหลือ 0 วินาที */
    }
    return seconds_left;
}

void FSM_Run(void) {
    /* รับ Event การกดปุ่มที่ EXTI Interrupt บันทึกไว้ (กด 1 ครั้ง = 1 Event ไม่ Repeat ขณะกดค้าง)
     * ดึงออกมาทุก Tick เสมอ แม้ระบบถูกล็อก เพื่อไม่ให้การกดระหว่างล็อกค้างไว้แล้วไปทำงานทีหลัง
     */
    uint8_t up_edge   = BTN_TakePress(BTN_UP_PORT,   BTN_UP_PIN);
    uint8_t down_edge = BTN_TakePress(BTN_DOWN_PORT, BTN_DOWN_PIN);
    uint8_t ok_edge   = BTN_TakePress(BTN_OK_PORT,   BTN_OK_PIN);
    /* ปุ่ม BACK แยก 2 แบบ: กดสั้น (ยกเลิก/ย้อนกลับ เหมือนเดิม) และกดค้าง 1.5 วินาที (เปิดหน้า SETTINGS) */
    uint8_t back_event = BTN_TakeBackEvent();
    uint8_t back_edge = 0U;
    uint8_t back_long = 0U;

    /* Event การบังแสง (หยอดเหรียญ) จาก EXTI ดึงออกทุก Tick เหมือนปุ่ม แต่นับเงินเฉพาะตอนอยู่ใน PAYMENT
     * เพื่อไม่ให้การบังแสงก่อนถึงหน้าชำระเงินถูกเก็บค้างไว้แล้วนับเป็นเงินทีหลัง
     */
    uint8_t coin_edge = LightSensor_TakeBlockEvent();

    /* Flag สภาวะแวดล้อมจาก safety.c (Background Task ที่อัปเดตทุก ๆ รอบอ่าน Sensor)
     * อ่านครั้งเดียวต่อ Tick เพื่อให้ทุก case ในรอบนี้เห็นค่าเดียวกัน ไม่มีโอกาส Race ระหว่าง case
     */
    uint8_t locked = Safety_IsLockout();

    if (back_event == BTN_EVENT_SHORT) {
        back_edge = 1U;
    } else if (back_event == BTN_EVENT_LONG) {
        back_long = 1U;
    } else {
        /* ไม่มี Event จากปุ่ม BACK */
    }

    state_tick++;

    switch (current_state) {

    case STATE_INIT:
        UART2_SendString("[FSM] INIT -> IDLE\r\n");
        FSM_EnterState(STATE_IDLE);
        break;

    case STATE_IDLE:
        if (back_long != 0U) {
            /* หน้าตั้งค่าเปิดได้แม้ระบบล็อกอยู่ เพื่อให้ผู้ดูแลแก้เกณฑ์ที่ตั้งผิดได้ทันที */
            FSM_OpenSettings();
        }
        else if (locked != 0U) {
            /* ระบบถูกล็อกจากสภาวะแวดล้อมผิดปกติ -> ไม่ตอบสนองปุ่มใด ๆ (ดู LED1-3 และ UART WARNING) */
        }
        else if ((ok_edge != 0U) || (up_edge != 0U)) {
            /* หน้า IDLE บนจอบอกว่า "PRESS OK / UP" จึงรับได้ทั้งสองปุ่ม */
            selected_index = 0U;
            UART2_SendString("[FSM] Welcome! Select an item (UP/DOWN = scroll, OK = confirm)\r\n");
            UART2_SendString("> ");
            UART2_SendString(menu[selected_index].name);
            UART2_SendString("\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        else {
            /* ไม่มี Event ใหม่ -> ไม่ต้องทำอะไร */
        }
        break;

    case STATE_SELECT_DRINK:
        if (locked != 0U) {
            /* ระงับการทำงานขณะสภาวะแวดล้อมผิดปกติ (ไม่ตอบสนอง UP/DOWN/OK/BACK) */
        }
        else if (up_edge != 0U) {
            /* เลื่อนขึ้น: ถ้าอยู่รายการแรกให้วนไปรายการสุดท้าย */
            if (selected_index == 0U) {
                selected_index = (uint8_t)(MENU_ITEM_COUNT - 1U);
            } else {
                selected_index = (uint8_t)(selected_index - 1U);
            }
            UART2_SendString("> ");
            UART2_SendString(menu[selected_index].name);
            UART2_SendString("\r\n");
        }
        else if (down_edge != 0U) {
            selected_index = (uint8_t)((selected_index + 1U) % MENU_ITEM_COUNT);
            UART2_SendString("> ");
            UART2_SendString(menu[selected_index].name);
            UART2_SendString("\r\n");
        }
        else if (ok_edge != 0U) {
            UART2_SendString("[FSM] Confirm order: ");
            UART2_SendString(menu[selected_index].name);
            UART2_SendString(" ? (OK = Yes, BACK = Cancel)\r\n");
            FSM_EnterState(STATE_CONFIRM);
        }
        else if (back_edge != 0U) {
            UART2_SendString("[FSM] Cancelled -> back to IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        else {
            /* ไม่มี Event ใหม่ -> ไม่ต้องทำอะไร */
        }
        break;

    case STATE_CONFIRM:
        if (locked != 0U) {
            /* ระงับการทำงานขณะสภาวะแวดล้อมผิดปกติ */
        }
        else if (ok_edge != 0U) {
            FSM_EnterState(STATE_CHECK_STOCK);
        }
        else if (back_edge != 0U) {
            UART2_SendString("[FSM] Cancelled -> back to selection\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        else {
            /* รอผู้ใช้กด OK หรือ BACK */
        }
        break;

    case STATE_CHECK_STOCK:
        if (locked != 0U) {
            /* เผื่อกรณีสภาวะแวดล้อมเพิ่งเปลี่ยนเป็นผิดปกติระหว่างขั้นตอนนี้พอดี (Race เล็กน้อย) */
            FSM_CancelForLockout("");
        }
        else if (menu[selected_index].stock > 0U) {
            UART2_SendString("[FSM] Stock OK -> PAYMENT: please pay ");
            UART2_SendUint((uint32_t)menu[selected_index].price);
            UART2_SendString(" THB (cover light sensor = 10 THB, BACK = cancel, timeout ");
            UART2_SendUint(PAYMENT_TIMEOUT_SECONDS);
            UART2_SendString(" s)\r\n");
            FSM_EnterState(STATE_PAYMENT);
        }
        else {
            UART2_SendString("[FSM] OUT OF STOCK! Please choose another item.\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        break;

    case STATE_PAYMENT:
        if (locked != 0U) {
            FSM_CancelForLockout(" during payment");
        }
        else if (back_edge != 0U) {
            UART2_SendString("[PAY] Cancelled by customer,");
            FSM_PrintRefund();
            UART2_SendString(" -> back to selection\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        else if (coin_edge != 0U) {
            paid_amount += COIN_VALUE_THB;
            UART2_SendString("[PAY] +");
            UART2_SendUint(COIN_VALUE_THB);
            UART2_SendString(" THB -> Paid: ");
            UART2_SendUint(paid_amount);
            UART2_SendString(" / ");
            UART2_SendUint((uint32_t)menu[selected_index].price);
            UART2_SendString(" THB\r\n");

            if (paid_amount >= (uint32_t)menu[selected_index].price) {
                UART2_SendString("[PAY] Payment complete! Change: ");
                UART2_SendUint(paid_amount - (uint32_t)menu[selected_index].price);
                UART2_SendString(" THB\r\n");
                FSM_EnterState(STATE_SAFETY_CHECK);
            } else {
                /* ยอดยังไม่ครบ: รอรับเงินต่อ */
            }
        }
        else if (state_tick >= PAYMENT_TIMEOUT_TICKS) {
            UART2_SendString("[PAY] TIMEOUT! Paid ");
            UART2_SendUint(paid_amount);
            UART2_SendString(" of ");
            UART2_SendUint((uint32_t)menu[selected_index].price);
            UART2_SendString(" THB -> PAYMENT FAILED, money returned\r\n");
            FSM_EnterState(STATE_PAYMENT_FAILED);
        }
        else {
            /* รอลูกค้าชำระเงิน */
        }
        break;

    case STATE_PAYMENT_FAILED:
        /* แสดงหน้าแจ้งเตือนค้างไว้ 3 วินาที แล้วกลับหน้าแรกเอง (ไม่ต้องกดปุ่ม) */
        if (state_tick >= PAYMENT_FAILED_TICKS) {
            UART2_SendString("[FSM] Returning to main menu -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        } else {
            /* ยังแสดงข้อความแจ้งเตือนอยู่ */
        }
        break;

    case STATE_SAFETY_CHECK:
        /* ไม่ต้องอ่าน ADC ซ้ำที่นี่ -> ใช้ผลของ Background Task (safety.c) ที่อัปเดตต่อเนื่องอยู่แล้ว */
        if (locked != 0U) {
            FSM_CancelForLockout("");
        } else {
            UART2_SendString("[FSM] Environment OK -> Preparing your item... ");
            UART2_SendUint(PROCESSING_SECONDS);
            UART2_SendString(" sec\r\n");
            FSM_EnterState(STATE_PROCESSING);
        }
        break;

    case STATE_PROCESSING:
        if (locked != 0U) {
            /* Safety First: สภาพแวดล้อมผิดปกติระหว่างจ่ายสินค้า -> หยุดจ่ายทันทีและยกเลิกรายการ
             * Stock จะถูกหักเฉพาะตอนจ่ายครบเวลาเท่านั้น (ด้านล่าง) การออกจาก State ตรงนี้จึงไม่หัก Stock
             * FSM_EnterState(STATE_FAULT) จะดับ LED4 ให้เอง
             */
            FSM_CancelForLockout(" during dispensing");
        } else {
            /* LED4 กระพริบระหว่างกำลังจ่ายสินค้า (Toggle ทุก ๆ ~0.5 วินาที) */
            if ((state_tick % LED4_BLINK_TICKS) == 0U) {
                LED_Toggle(LED4_PORT, LED4_PIN);
            } else {
                /* No action */
            }

            /* Dispense Simulation: พิมพ์เลขนับถอยหลังทุก ๆ 1 วินาทีโดยประมาณ */
            if ((state_tick % TICKS_PER_SECOND) == 0U) {
                uint32_t seconds_elapsed = state_tick / TICKS_PER_SECOND;
                if (seconds_elapsed < (uint32_t)PROCESSING_SECONDS) {
                    uint32_t seconds_left = (uint32_t)PROCESSING_SECONDS - seconds_elapsed;
                    UART2_SendString("   ... ");
                    UART2_SendUint(seconds_left);
                    UART2_SendString(" sec\r\n");
                } else {
                    /* No action */
                }
            } else {
                /* No action */
            }

            if (state_tick >= PROCESSING_TICKS) {
                /* Stock Update: ตัด Stock ลง 1 หน่วยเมื่อจ่ายสินค้าสำเร็จ (กันค่าติดลบด้วยเงื่อนไข > 0) */
                if (menu[selected_index].stock > 0U) {
                    menu[selected_index].stock--;
                } else {
                    /* No action */
                }

                UART2_SendString("[FSM] Item prepared! Stock updated:\r\n");
                Menu_PrintAll();

                FSM_EnterState(STATE_COMPLETE);
            } else {
                /* No action */
            }
        }
        break;

    case STATE_COMPLETE:
        if (state_tick >= COMPLETE_TICKS) {
            UART2_SendString("[FSM] Enjoy your fresh pick! -> back to IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        } else {
            /* No action */
        }
        break;

    case STATE_SETTINGS:
        /* หน้าตั้งค่ายังทำงานต่อแม้ระบบล็อก (ผู้ดูแลต้องแก้เกณฑ์ได้) การตรวจ Safety ยังทำงานเบื้องหลังตามปกติ */
        if (Settings_TakeActivity() != 0U) {
            state_tick = 0U;   /* หมุนปุ่ม = มีการใช้งาน: เริ่มนับ Timeout ใหม่ */
        } else {
            /* ไม่ได้หมุนปุ่ม */
        }

        if (ok_edge != 0U) {
            Settings_Save();
            UART2_SendString("[FSM] Settings saved -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        else if (back_edge != 0U) {
            Settings_Exit();
            UART2_SendString("[FSM] Settings cancelled (not saved) -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        else if ((up_edge != 0U) || (down_edge != 0U)) {
            Settings_NextField();
            state_tick = 0U;
        }
        else if (state_tick >= SETTINGS_TIMEOUT_TICKS) {
            Settings_Exit();
            UART2_SendString("[FSM] Settings timeout (not saved) -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        else {
            /* รอผู้ใช้ */
        }
        break;

    case STATE_FAULT:
        /* Auto-Recovery: ไม่ต้องกด BACK เอง -> กลับ IDLE ทันทีที่สภาวะแวดล้อมกลับมาปกติ
         * (ข้อความ "[SYSTEM] Environment Restored - System Ready" พิมพ์แล้วโดย safety.c)
         * กด BACK ค้างระหว่างล็อกเพื่อเปิดหน้าตั้งค่าได้ (เผื่อเกณฑ์ตั้งไว้ต่ำเกินไป)
         */
        if (back_long != 0U) {
            FSM_OpenSettings();
        } else if (locked == 0U) {
            UART2_SendString("[FSM] Resuming normal operation -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        } else {
            /* No action */
        }
        break;

    default:
        FSM_EnterState(STATE_FAULT);
        break;
    }
}

/* ผูก LED4 (DISPENSING/BUSY STATUS) เข้ากับ State ปัจจุบัน (เรียกทุกครั้งที่เปลี่ยน State เท่านั้น)
 * หมายเหตุ (ฉบับปรับปรุง): LED1-3 ไม่ใช่หน้าที่ของ FSM อีกต่อไป ถูกควบคุมแยกโดย safety.c
 * แบบ Background Task ต่อเนื่อง เพื่อไม่ให้การเปลี่ยน State ของ FSM ไปกระทบไฟเตือนสภาวะแวดล้อม
 */
static void FSM_UpdateOutputs(void) {
    switch (current_state) {
    case STATE_CHECK_STOCK:
    case STATE_SAFETY_CHECK:
    case STATE_PROCESSING:
    case STATE_COMPLETE:
        LED_On(LED4_PORT, LED4_PIN);   /* กำลังประมวลผล/จ่ายสินค้า (ระหว่าง PROCESSING จะถูก Toggle ให้กระพริบเพิ่มใน FSM_Run) */
        break;

    default:
        LED_Off(LED4_PORT, LED4_PIN);  /* IDLE / SELECT_DRINK / CONFIRM / FAULT: ไม่ได้กำลังจ่ายสินค้า */
        break;
    }
}

static void FSM_EnterState(SystemState_t new_state) {
    current_state = new_state;
    if (new_state == STATE_IDLE) {
        order_cancelled = 0U; /* เริ่มรายการใหม่ ล้างสถานะการยกเลิกของรายการก่อนหน้า */
        paid_amount = 0U;
    } else if (new_state == STATE_PAYMENT) {
        paid_amount = 0U;     /* เริ่มรับเงินรายการใหม่จาก 0 บาท */
    } else {
        /* No action */
    }
    state_tick = 0U;
    FSM_UpdateOutputs();
}

/* เปิดหน้า SETTINGS (จาก IDLE หรือ FAULT ด้วยการกด BACK ค้าง) */
static void FSM_OpenSettings(void) {
    Settings_Enter();
    UART2_SendString("[FSM] Settings mode (UP/DOWN = select, knob = adjust, OK = save, BACK = cancel)\r\n");
    FSM_EnterState(STATE_SETTINGS);
}

/* ยกเลิกรายการเพราะสภาพแวดล้อมผิดปกติ (Safety First): ไม่หัก Stock และคืนเงินที่ลูกค้าจ่ายมาแล้ว
 * stage_text ใช้บอกว่าเกิดขึ้นช่วงไหน เช่น " during payment", " during dispensing" ("" = ไม่ระบุ)
 */
static void FSM_CancelForLockout(const char *stage_text) {
    UART2_SendString("[FSM] Environment alert");
    UART2_SendString(stage_text);
    UART2_SendString(" -> ORDER CANCELLED (stock not deducted");
    if (paid_amount > 0U) {
        UART2_SendString(",");
        FSM_PrintRefund();
    } else {
        /* ยังไม่ได้รับเงิน: ไม่มีเงินต้องคืน */
    }
    UART2_SendString(")\r\n");
    order_cancelled = 1U;
    FSM_EnterState(STATE_FAULT);
}

/* พิมพ์ยอดคืนเงิน " refund X THB" (ระบบจำลอง: ไม่มีกลไกคืนเหรียญจริง แจ้งยอดผ่าน UART/จอแทน) */
static void FSM_PrintRefund(void) {
    UART2_SendString(" refund ");
    UART2_SendUint(paid_amount);
    UART2_SendString(" THB");
}
