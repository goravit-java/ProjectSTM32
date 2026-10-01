#include "Drivers/exti_driver.h"
#include "Drivers/cortex_driver.h"
#include "Drivers/tim2_driver.h"

#define EXTI_LINE_COUNT   16U

/* สถานะของแต่ละ EXTI Line (index = เลขขา 0-15)
 * exti_pending : ISR ตั้งเป็น 1 เมื่อรับ Edge, EXTI_TakeEvent() เคลียร์เป็น 0
 * exti_stamp   : เวลา (us จาก TIM2) ของ Edge ล่าสุดที่รับไว้
 * exti_stamped : 1 = exti_stamp มีค่าจริงแล้ว (ใช้ตอนเริ่มระบบ ยังไม่เคยมี Edge)
 */
static volatile uint8_t exti_pending[EXTI_LINE_COUNT];
static volatile uint32_t exti_stamp[EXTI_LINE_COUNT];
static volatile uint8_t exti_stamped[EXTI_LINE_COUNT];

static void EXTI_HandleLine(uint8_t line);
static uint8_t EXTI_PortCode(const GPIO_TypeDef *port);
static uint8_t EXTI_IrqForLine(uint8_t line);

void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void EXTI15_10_IRQHandler(void);

void EXTI_InitFallingEdge(GPIO_TypeDef *port, uint8_t pin) {
    uint32_t cr_index;
    uint32_t cr_shift;
    uint32_t line_bit;

    if (pin < EXTI_LINE_COUNT) {
        cr_index = (uint32_t)pin / 4U;
        cr_shift = ((uint32_t)pin % 4U) * 4U;
        line_bit = 1U << (uint32_t)pin;

        exti_pending[pin] = 0U;
        exti_stamped[pin] = 0U;

        /* 1. เปิด Clock ให้ SYSCFG แล้วเลือกว่า EXTI Line นี้รับสัญญาณจาก Port ไหน */
        RCC->APB2ENR |= (1U << 14U); /* SYSCFGEN */
        SYSCFG->EXTICR[cr_index] &= ~(0xFU << cr_shift);
        SYSCFG->EXTICR[cr_index] |= ((uint32_t)EXTI_PortCode(port) << cr_shift);

        /* 2. ตรวจจับเฉพาะขอบขาลง (กดปุ่ม Active-Low) ไม่สนขอบขาขึ้น (ปล่อยปุ่ม) */
        EXTI->FTSR |= line_bit;
        EXTI->RTSR &= ~line_bit;

        /* 3. เคลียร์ Pending ที่อาจค้างอยู่ แล้วเปิด Interrupt Mask ของ Line นี้ */
        EXTI->PR = line_bit;
        EXTI->IMR |= line_bit;

        /* 4. เปิด IRQ ของกลุ่ม Line นี้ใน NVIC */
        Cortex_NvicEnableIrq(EXTI_IrqForLine(pin));
    }
}

uint8_t EXTI_TakeEvent(uint8_t pin) {
    uint8_t result = 0U;

    if ((pin < EXTI_LINE_COUNT) && (exti_pending[pin] != 0U)) {
        if ((TIM2_GetMicros() - exti_stamp[pin]) >= EXTI_SETTLE_US) {
            exti_pending[pin] = 0U;
            result = 1U;
        }
    }
    return result;
}

/* งานร่วมของทุก EXTI ISR: เคลียร์ Pending Bit (เขียน 1 = เคลียร์) แล้วบันทึก Event ถ้าพ้นช่วง Guard */
static void EXTI_HandleLine(uint8_t line) {
    uint32_t line_bit = 1U << (uint32_t)line;
    uint32_t now;

    if ((EXTI->PR & line_bit) != 0U) {
        EXTI->PR = line_bit;
        now = TIM2_GetMicros();

        if ((exti_stamped[line] == 0U) || ((now - exti_stamp[line]) >= EXTI_GUARD_US)) {
            exti_stamp[line] = now;
            exti_stamped[line] = 1U;
            exti_pending[line] = 1U;
        }
    }
}

static uint8_t EXTI_PortCode(const GPIO_TypeDef *port) {
    uint8_t code;

    if (port == GPIOA) {
        code = 0U;
    } else if (port == GPIOB) {
        code = 1U;
    } else if (port == GPIOC) {
        code = 2U;
    } else {
        code = 0U; /* Port ที่ไม่รองรับในโปรเจกต์นี้: ใช้ค่า Default ของ Hardware (PA) */
    }
    return code;
}

/* Line 0-4 มี IRQ ของตัวเอง (6-10), Line 5-9 ใช้ IRQ ร่วมกัน, Line 10-15 ใช้ IRQ ร่วมกัน */
static uint8_t EXTI_IrqForLine(uint8_t line) {
    uint8_t irqn;

    if (line <= 4U) {
        irqn = (uint8_t)(6U + line);
    } else if (line <= 9U) {
        irqn = (uint8_t)EXTI9_5_IRQN;
    } else {
        irqn = (uint8_t)EXTI15_10_IRQN;
    }
    return irqn;
}

/* ชื่อฟังก์ชันด้านล่างต้องตรงกับ Vector Table ใน startup_stm32f411retx.s ทุกตัวอักษร */
void EXTI3_IRQHandler(void) {
    EXTI_HandleLine(3U);
}

void EXTI4_IRQHandler(void) {
    EXTI_HandleLine(4U);
}

void EXTI9_5_IRQHandler(void) {
    uint8_t line;

    for (line = 5U; line <= 9U; line++) {
        EXTI_HandleLine(line);
    }
}

void EXTI15_10_IRQHandler(void) {
    uint8_t line;

    for (line = 10U; line <= 15U; line++) {
        EXTI_HandleLine(line);
    }
}
