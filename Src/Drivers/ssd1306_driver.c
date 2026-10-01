#include "Drivers/ssd1306_driver.h"
#include "Drivers/i2c_driver.h"
#include "Drivers/font5x7.h"

/* Framebuffer ในหน่วยความจำ: 128x64 พิกเซล เก็บแบบ 1 บิต/พิกเซล รวม 1024 Byte
 * แบ่งเป็น 8 Page แนวตั้ง (Page ละ 8 พิกเซล) ตามโครงสร้าง RAM จริงของ SSD1306
 */
static uint8_t ssd1306_buffer[(SSD1306_WIDTH * SSD1306_HEIGHT) / 8U];

static void SSD1306_SendCommand(uint8_t cmd);

void SSD1306_Init(void) {
    SoftI2C_Init();

    SSD1306_SendCommand(0xAEU); /* Display OFF ระหว่างตั้งค่า */
    SSD1306_SendCommand(0xD5U);
    SSD1306_SendCommand(0x80U); /* Clock Divide / Oscillator Frequency */
    SSD1306_SendCommand(0xA8U);
    SSD1306_SendCommand(0x3FU); /* Multiplex Ratio = 64 (สำหรับจอ 128x64) */
    SSD1306_SendCommand(0xD3U);
    SSD1306_SendCommand(0x00U); /* Display Offset = 0 */
    SSD1306_SendCommand(0x40U); /* Display Start Line = 0 */
    SSD1306_SendCommand(0x8DU);
    SSD1306_SendCommand(0x14U); /* เปิด Charge Pump (จำเป็นสำหรับโมดูลที่ไม่มีไฟ VCC แยกสำหรับ Panel) */
    SSD1306_SendCommand(0x20U);
    SSD1306_SendCommand(0x02U); /* Memory Addressing Mode = Page Addressing Mode */
    SSD1306_SendCommand(0xA1U); /* Segment Remap (กลับซ้าย-ขวา ให้ตรงกับการต่อ Panel จริง) */
    SSD1306_SendCommand(0xC8U); /* COM Output Scan Direction (กลับบน-ล่าง) */
    SSD1306_SendCommand(0xDAU);
    SSD1306_SendCommand(0x12U); /* COM Pins Hardware Configuration */
    SSD1306_SendCommand(0x81U);
    SSD1306_SendCommand(0x8FU); /* Contrast Control */
    SSD1306_SendCommand(0xD9U);
    SSD1306_SendCommand(0xF1U); /* Pre-charge Period */
    SSD1306_SendCommand(0xDBU);
    SSD1306_SendCommand(0x40U); /* VCOMH Deselect Level */
    SSD1306_SendCommand(0xA4U); /* Entire Display ON: แสดงตาม RAM จริง (ไม่ใช่ All-pixel-ON) */
    SSD1306_SendCommand(0xA6U); /* Normal Display (ไม่กลับสี) */
    SSD1306_SendCommand(0xAFU); /* Display ON */

    SSD1306_Clear();
    SSD1306_UpdateScreen();
}

static void SSD1306_SendCommand(uint8_t cmd) {
    uint8_t packet[2];

    packet[0] = 0x00U; /* Control Byte: Co=0, D/C#=0 -> ไบต์ถัดไปคือ Command */
    packet[1] = cmd;
    (void)SoftI2C_WriteBytes(SSD1306_I2C_ADDR, packet, 2U);
}

void SSD1306_Clear(void) {
    uint32_t i;

    for (i = 0U; i < sizeof(ssd1306_buffer); i++) {
        ssd1306_buffer[i] = 0x00U;
    }
}

void SSD1306_UpdateScreen(void) {
    uint8_t page;

    for (page = 0U; page < (uint8_t)SSD1306_PAGE_COUNT; page++) {
        uint8_t packet[1U + SSD1306_WIDTH]; /* [0] = Control Byte (0x40 = Data), [1..128] = ข้อมูล 1 Page */
        uint16_t i;

        SSD1306_SendCommand((uint8_t)(0xB0U + page)); /* เลือก Page ที่จะเขียน (0-7) */
        SSD1306_SendCommand(0x00U);                    /* Column Address: Low Nibble = 0 */
        SSD1306_SendCommand(0x10U);                    /* Column Address: High Nibble = 0 */

        packet[0] = 0x40U; /* Control Byte: Co=0, D/C#=1 -> ไบต์ถัดไปคือ Data */
        for (i = 0U; i < (uint16_t)SSD1306_WIDTH; i++) {
            packet[1U + i] = ssd1306_buffer[((uint32_t)page * SSD1306_WIDTH) + i];
        }
        (void)SoftI2C_WriteBytes(SSD1306_I2C_ADDR, packet, 1U + (uint16_t)SSD1306_WIDTH);
    }
}

void SSD1306_SetPixel(uint16_t x, uint16_t y, uint8_t color) {
    uint32_t index;

    if ((x >= (uint16_t)SSD1306_WIDTH) || (y >= (uint16_t)SSD1306_HEIGHT)) {
        return; /* อยู่นอกขอบจอ ไม่ต้องทำอะไร (ป้องกัน Buffer Overflow) */
    }

    index = (uint32_t)x + (((uint32_t)y / 8U) * SSD1306_WIDTH);

    if (color != 0U) {
        ssd1306_buffer[index] |= (uint8_t)(1U << (y % 8U));
    } else {
        ssd1306_buffer[index] &= (uint8_t)(~(1U << (y % 8U)));
    }
}

void SSD1306_DrawChar(uint16_t x, uint16_t y, char c) {
    uint8_t ch = (uint8_t)c;
    uint8_t col;

    if ((ch < (uint8_t)FONT_FIRST_CHAR) || (ch > (uint8_t)FONT_LAST_CHAR)) {
        ch = (uint8_t)'?'; /* ตัวอักษรที่ไม่มีในตารางฟอนต์ ใช้ '?' แทน กันไม่ให้วาดเพี้ยน */
    }

    for (col = 0U; col < (uint8_t)FONT_WIDTH; col++) {
        uint8_t line = font5x7_table[ch - (uint8_t)FONT_FIRST_CHAR][col];
        uint8_t row;

        for (row = 0U; row < (uint8_t)FONT_HEIGHT; row++) {
            uint8_t bit = (uint8_t)((line >> row) & 0x01U);
            SSD1306_SetPixel((uint16_t)(x + col), (uint16_t)(y + row), bit);
        }
    }
}

uint16_t SSD1306_DrawString(uint16_t x, uint16_t y, const char *str) {
    uint16_t cursor_x = x;
    const char *p = str; /* MISRA 17.8: ไม่แก้ค่า Parameter โดยตรง ใช้ตัวแปรสำเนาแทน */

    while (*p != '\0') {
        SSD1306_DrawChar(cursor_x, y, *p);
        cursor_x = (uint16_t)(cursor_x + (uint16_t)FONT_WIDTH + 1U); /* +1 = ช่องว่างระหว่างตัวอักษร */
        p++;
    }
    return cursor_x;
}

/* วาดจำนวนเต็มไม่ติดลบ โดยไม่ใช้ sprintf (Logic เดียวกับ UART2_SendUint แต่วาดลงจอแทนการส่ง UART) */
uint16_t SSD1306_DrawUint(uint16_t x, uint16_t y, uint32_t value) {
    char buf[10];
    uint8_t i = 0U;
    uint16_t cursor_x = x;
    uint32_t v = value; /* MISRA 17.8: ทำงานกับสำเนา ไม่แก้ Parameter */

    if (v == 0U) {
        SSD1306_DrawChar(cursor_x, y, '0');
        cursor_x = (uint16_t)(cursor_x + (uint16_t)FONT_WIDTH + 1U);
    } else {
        while (v > 0U) {
            buf[i] = (char)((v % 10U) + (uint32_t)'0');
            i++;
            v /= 10U;
        }

        while (i > 0U) {
            i--;
            SSD1306_DrawChar(cursor_x, y, buf[i]);
            cursor_x = (uint16_t)(cursor_x + (uint16_t)FONT_WIDTH + 1U);
        }
    }
    return cursor_x;
}

/* วาดค่า float ทศนิยม 1 ตำแหน่ง (เช่น "43.5") Logic เดียวกับ UART2_SendFloat1 (ปัดเศษถูกต้องรวมกรณีทด) */
uint16_t SSD1306_DrawFloat1(uint16_t x, uint16_t y, float value) {
    int32_t integer_part;
    int32_t decimal_part;
    uint16_t cursor_x = x;
    float v = value; /* MISRA 17.8: ทำงานกับสำเนา ไม่แก้ Parameter */

    if (v < 0.0f) {
        SSD1306_DrawChar(cursor_x, y, '-');
        cursor_x = (uint16_t)(cursor_x + (uint16_t)FONT_WIDTH + 1U);
        v = -v;
    }

    integer_part = (int32_t)v;
    decimal_part = (int32_t)(((v - (float)integer_part) * 10.0f) + 0.5f);

    if (decimal_part >= 10) {
        decimal_part = 0;
        integer_part++;
    }

    cursor_x = SSD1306_DrawUint(cursor_x, y, (uint32_t)integer_part);
    SSD1306_DrawChar(cursor_x, y, '.');
    cursor_x = (uint16_t)(cursor_x + (uint16_t)FONT_WIDTH + 1U);
    cursor_x = SSD1306_DrawUint(cursor_x, y, (uint32_t)decimal_part);
    return cursor_x;
}

void SSD1306_DrawHLine(uint16_t x, uint16_t y, uint16_t length) {
    uint16_t i;

    for (i = 0U; i < length; i++) {
        SSD1306_SetPixel((uint16_t)(x + i), y, 1U);
    }
}

void SSD1306_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t i;

    for (i = 0U; i < w; i++) {
        SSD1306_SetPixel((uint16_t)(x + i), y, 1U);
        SSD1306_SetPixel((uint16_t)(x + i), (uint16_t)(y + h - 1U), 1U);
    }
    for (i = 0U; i < h; i++) {
        SSD1306_SetPixel(x, (uint16_t)(y + i), 1U);
        SSD1306_SetPixel((uint16_t)(x + w - 1U), (uint16_t)(y + i), 1U);
    }
}

void SSD1306_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < w; i++) {
        for (j = 0U; j < h; j++) {
            SSD1306_SetPixel((uint16_t)(x + i), (uint16_t)(y + j), 1U);
        }
    }
}
