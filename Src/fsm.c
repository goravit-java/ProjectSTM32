#include "fsm.h"
#include "gpio_driver.h"
#include "uart_driver.h"
#include "adc_driver.h"
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

    state_tick++;

    switch (current_state) {

    case STATE_INIT:
        UART2_SendString("[FSM] INIT -> IDLE\r\n");
        FSM_EnterState(STATE_IDLE);
        break;

    case STATE_IDLE:
        if (ok_edge != 0U) {
            selected_index = 0U;
            UART2_SendString("[FSM] Welcome! Select a drink (UP/DOWN = scroll, OK = confirm)\r\n");
            UART2_SendString("> ");
            UART2_SendString(menu[selected_index].name);
            UART2_SendString("\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        break;

    case STATE_SELECT_DRINK:
        if (up_edge != 0U) {
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
        if (ok_edge != 0U) {
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
        if (menu[selected_index].stock > 0U) {
            UART2_SendString("[FSM] Stock OK -> checking safety...\r\n");
            FSM_EnterState(STATE_SAFETY_CHECK);
        } else {
            UART2_SendString("[FSM] OUT OF STOCK! Please choose another drink.\r\n");
            FSM_EnterState(STATE_SELECT_DRINK);
        }
        break;

    case STATE_SAFETY_CHECK: {
        float temp = ADC1_ReadTemperature();
        UART2_PrintTemperature(temp);

        if (Safety_CheckTemperature(temp) != 0U) {
            UART2_SendString("[FSM] Preparing your drink... ");
            UART2_SendUint(PROCESSING_SECONDS);
            UART2_SendString(" sec\r\n");
            FSM_EnterState(STATE_PROCESSING);
        } else {
            FSM_EnterState(STATE_FAULT);
        }
        break;
    }

    case STATE_PROCESSING:
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
            /* Stock Update: ตัด Stock ลง 1 หน่วยเมื่อจ่ายเครื่องดื่มสำเร็จ (กันค่าติดลบด้วยเงื่อนไข > 0) */
            if (menu[selected_index].stock > 0U) {
                menu[selected_index].stock--;
            }

            UART2_SendString("[FSM] Drink prepared! Stock updated:\r\n");
            Menu_PrintAll();

            FSM_EnterState(STATE_COMPLETE);
        }
        break;

    case STATE_COMPLETE:
        if (state_tick >= COMPLETE_TICKS) {
            UART2_SendString("[FSM] Enjoy your drink! -> back to IDLE\r\n");
            FSM_EnterState(STATE_IDLE);
        }
        break;

    case STATE_FAULT:
        if (back_edge != 0U) {
            UART2_SendString("[FSM] Fault acknowledged -> back to IDLE\r\n");
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

/* ผูกสถานะ LED เข้ากับ State ปัจจุบัน (เรียกทุกครั้งที่เปลี่ยน State เท่านั้น ไม่ใช่ทุก Loop) */
static void FSM_UpdateOutputs(void) {
    LED_Off(LED1_PORT, LED1_PIN);
    LED_Off(LED2_PORT, LED2_PIN);
    LED_Off(LED3_PORT, LED3_PIN);
    LED_Off(LED4_PORT, LED4_PIN);

    switch (current_state) {
    case STATE_IDLE:
        LED_On(LED1_PORT, LED1_PIN);   /* ระบบพร้อมใช้งาน */
        break;

    case STATE_SELECT_DRINK:
    case STATE_CONFIRM:
        LED_On(LED2_PORT, LED2_PIN);   /* กำลังเลือก/ยืนยันเมนู */
        break;

    case STATE_CHECK_STOCK:
    case STATE_SAFETY_CHECK:
    case STATE_PROCESSING:
    case STATE_COMPLETE:
        LED_On(LED3_PORT, LED3_PIN);   /* กำลังประมวลผล/เตรียมเครื่องดื่ม */
        break;

    case STATE_FAULT:
        LED_On(LED4_PORT, LED4_PIN);   /* สถานะผิดพลาด */
        break;

    default:
        break;
    }
}

static void FSM_EnterState(SystemState_t new_state) {
    current_state = new_state;
    state_tick = 0U;
    FSM_UpdateOutputs();
}
