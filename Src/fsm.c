#include "fsm.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "safety.h"
#include "menu.h"

/* Main loop เรียก FSM_Run() ทุก ๆ ~20ms (ดู delay_ms(20) ใน main.c)
 * ค่า TICKS ด้านล่างจึงอิงจากรอบเวลานั้น (เป็นค่าประมาณ เพราะยังไม่มี Hardware Timer/SysTick จริง)
 * ใช้แทนการ Block ด้วย delay_ms ตรง ๆ เพื่อไม่ให้ IWDG_Refresh() และการอ่านปุ่มค้างระหว่างรอ
 */
#define TICKS_PER_SECOND     50U /* 1000ms / 20ms ต่อรอบ Loop */
#define PROCESSING_SECONDS    4U /* Dispense Simulation: นับถอยหลัง 4 วินาที (อยู่ในช่วง 3-5 วิ ตามสเปก) */
#define PROCESSING_TICKS     (PROCESSING_SECONDS * TICKS_PER_SECOND)
#define COMPLETE_TICKS        (1U * TICKS_PER_SECOND) /* ~1 วินาที : เวลาแสดงผล "เสร็จสิ้น" ก่อนกลับ IDLE */

static SystemState_t current_state = STATE_INIT;
static uint8_t selected_index = 0U;
static uint32_t state_tick = 0U;

/* เก็บสถานะปุ่มของรอบก่อนหน้า เพื่อตรวจจับ Edge (กด 1 ครั้ง = 1 Event เท่านั้น ไม่ Repeat ขณะกดค้าง) */
static uint8_t prev_up = 0U;
static uint8_t prev_down = 0U;
static uint8_t prev_ok = 0U;
static uint8_t prev_back = 0U;

static uint8_t EdgePressed(GPIO_TypeDef *port, uint8_t pin, uint8_t *prev_state);
static void FSM_UpdateOutputs(void);
static void FSM_EnterState(SystemState_t new_state);

void FSM_Init(void) {
    current_state = STATE_INIT;
    selected_index = 0U;
    state_tick = 0U;
    prev_up = 0U;
    prev_down = 0U;
    prev_ok = 0U;
    prev_back = 0U;
}

SystemState_t FSM_GetState(void) {
    return current_state;
}

void FSM_Run(void) {
    uint8_t up_edge   = EdgePressed(BTN_UP_PORT,   BTN_UP_PIN,   &prev_up);
    uint8_t down_edge = EdgePressed(BTN_DOWN_PORT, BTN_DOWN_PIN, &prev_down);
    uint8_t ok_edge   = EdgePressed(BTN_OK_PORT,   BTN_OK_PIN,   &prev_ok);
    uint8_t back_edge = EdgePressed(BTN_BACK_PORT, BTN_BACK_PIN, &prev_back);

    /* Flag สภาวะแวดล้อมจาก safety.c (Background Task ที่อัปเดตทุก ๆ รอบอ่าน Sensor)
     * อ่านครั้งเดียวต่อ Tick เพื่อให้ทุก case ในรอบนี้เห็นค่าเดียวกัน ไม่มีโอกาส Race ระหว่าง case
     */
    uint8_t locked = Safety_IsLockout();

    state_tick++;

    switch (current_state) {

    case STATE_INIT:
        UART2_SendString("[FSM] INIT -> IDLE\r\n");
        FSM_EnterState(STATE_IDLE);
        break;

    case STATE_IDLE:
        if (locked != 0U) {
            /* ระบบถูกล็อกจากสภาวะแวดล้อมผิดปกติ -> ไม่ตอบสนองปุ่มใด ๆ (ดู LED1-3 และ UART WARNING) */
        }
        else if (ok_edge != 0U) {
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
            selected_index = (selected_index == 0U) ? (uint8_t)(MENU_ITEM_COUNT - 1U) : (uint8_t)(selected_index - 1U);
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
            UART2_SendString("[FSM] Environment alert triggered mid-order -> aborting\r\n");
            FSM_EnterState(STATE_FAULT);
        }
        else if (menu[selected_index].stock > 0U) {
            UART2_SendString("[FSM] Stock OK -> checking safety...\r\n");
            FSM_EnterState(STATE_SAFETY_CHECK);
        }
        else {
            UART2_SendString("[FSM] OUT OF STOCK! Please choose another item.\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        break;

    case STATE_SAFETY_CHECK:
        /* ไม่ต้องอ่าน ADC ซ้ำที่นี่ -> ใช้ผลของ Background Task (safety.c) ที่อัปเดตต่อเนื่องอยู่แล้ว */
        if (locked != 0U) {
            FSM_EnterState(STATE_FAULT);
        } else {
            UART2_SendString("[FSM] Environment OK -> Preparing your item... ");
            UART2_SendUint(PROCESSING_SECONDS);
            UART2_SendString(" sec\r\n");
            FSM_EnterState(STATE_PROCESSING);
        }
        break;

    case STATE_PROCESSING:
        /* LED4 กระพริบระหว่างกำลังจ่ายสินค้า (Toggle ทุก ๆ ~0.5 วินาที) */
        if ((state_tick % 25U) == 0U) {
            LED_Toggle(LED4_PORT, LED4_PIN);
        }

        /* Dispense Simulation: พิมพ์เลขนับถอยหลังทุก ๆ 1 วินาทีโดยประมาณ */
        if ((state_tick % TICKS_PER_SECOND) == 0U) {
            uint32_t seconds_elapsed = state_tick / TICKS_PER_SECOND;
            if (seconds_elapsed < (uint32_t)PROCESSING_SECONDS) {
                uint32_t seconds_left = (uint32_t)PROCESSING_SECONDS - seconds_elapsed;
                UART2_SendString("   ... ");
                UART2_SendUint(seconds_left);
                UART2_SendString(" sec\r\n");
            }
        }

        if (state_tick >= PROCESSING_TICKS) {
            /* Stock Update: ตัด Stock ลง 1 หน่วยเมื่อจ่ายสินค้าสำเร็จ (กันค่าติดลบด้วยเงื่อนไข > 0) */
            if (menu[selected_index].stock > 0U) {
                menu[selected_index].stock--;
            }

            UART2_SendString("[FSM] Item prepared! Stock updated:\r\n");
            Menu_PrintAll();

            FSM_EnterState(STATE_COMPLETE);
        }
        break;

    case STATE_COMPLETE:
        if (state_tick >= COMPLETE_TICKS) {
            UART2_SendString("[FSM] Enjoy your fresh pick! -> back to IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        break;

    case STATE_FAULT:
        /* Auto-Recovery: ไม่ต้องกด BACK เอง -> กลับ IDLE ทันทีที่สภาวะแวดล้อมกลับมาปกติ
         * (ข้อความ "[SYSTEM] Environment Restored - System Ready" พิมพ์แล้วโดย safety.c)
         */
        if (locked == 0U) {
            UART2_SendString("[FSM] Resuming normal operation -> IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        break;

    default:
        FSM_EnterState(STATE_FAULT);
        break;
    }
}

/* ตรวจจับ Rising Edge ของปุ่ม (Active-Low ผ่าน BTN_IsPressed) -> คืนค่า 1 แค่ตอนเพิ่งกดครั้งแรกเท่านั้น */
static uint8_t EdgePressed(GPIO_TypeDef *port, uint8_t pin, uint8_t *prev_state) {
    uint8_t current = BTN_IsPressed(port, pin);
    uint8_t edge = ((current != 0U) && (*prev_state == 0U)) ? 1U : 0U;
    *prev_state = current;
    return edge;
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
    state_tick = 0U;
    FSM_UpdateOutputs();
}
