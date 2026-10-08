#include "Drivers/ssd1306_driver.h"
#include "Drivers/i2c_driver.h"
#include "Drivers/font5x7.h"

/* ---------------- คำสั่งของ SSD1306 (Datasheet: Command Table) ---------------- */
#define SSD1306_CMD_DISPLAY_OFF        0xAEU
#define SSD1306_CMD_DISPLAY_ON         0xAFU
#define SSD1306_CMD_SET_CLOCK_DIV      0xD5U
#define SSD1306_CLOCK_DIV_DEFAULT      0x80U   /* ค่าแนะนำจาก Datasheet */
#define SSD1306_CMD_SET_MULTIPLEX      0xA8U
#define SSD1306_MULTIPLEX_64           0x3FU   /* 64 แถว - 1 */
#define SSD1306_CMD_SET_OFFSET         0xD3U
#define SSD1306_OFFSET_NONE            0x00U
#define SSD1306_CMD_START_LINE_0       0x40U
#define SSD1306_CMD_CHARGE_PUMP        0x8DU
#define SSD1306_CHARGE_PUMP_ON         0x14U
#define SSD1306_CMD_MEMORY_MODE        0x20U
#define SSD1306_MEMORY_MODE_PAGE       0x02U
#define SSD1306_CMD_SEG_REMAP          0xA1U   /* กลับซ้าย-ขวา ให้ตรงกับการต่อ Panel จริง */
#define SSD1306_CMD_COM_SCAN_DEC       0xC8U   /* กลับบน-ล่าง */
#define SSD1306_CMD_SET_COM_PINS       0xDAU
#define SSD1306_COM_PINS_ALT           0x12U
#define SSD1306_CMD_SET_CONTRAST       0x81U
#define SSD1306_CONTRAST_LEVEL         0x8FU
#define SSD1306_CMD_SET_PRECHARGE      0xD9U
#define SSD1306_PRECHARGE_PERIOD       0xF1U
#define SSD1306_CMD_SET_VCOMH          0xDBU
#define SSD1306_VCOMH_LEVEL            0x40U
#define SSD1306_CMD_RESUME_RAM         0xA4U   /* แสดงตาม RAM จริง (ไม่ใช่ All-pixel-ON) */
#define SSD1306_CMD_NORMAL_DISPLAY     0xA6U   /* ไม่กลับสี */
#define SSD1306_CMD_PAGE_START         0xB0U   /* 0xB0 + เลข Page (0-7) */
#define SSD1306_CMD_COL_LOW_0          0x00U
#define SSD1306_CMD_COL_HIGH_0         0x10U

/* Control Byte ที่นำหน้าทุกข้อความ I2C */
#define SSD1306_CTRL_COMMAND           0x00U   /* Co=0, D/C#=0 -> ไบต์ถัดไปคือ Command */
#define SSD1306_CTRL_DATA              0x40U   /* Co=0, D/C#=1 -> ไบต์ถัดไปคือ Data */
#define SSD1306_CMD_PACKET_LEN         2U

#define SSD1306_PIXELS_PER_PAGE        8U
#define SSD1306_BUFFER_SIZE            ((SSD1306_WIDTH * SSD1306_HEIGHT) / SSD1306_PIXELS_PER_PAGE)
#define SSD1306_DATA_PACKET_LEN        (1U + SSD1306_WIDTH)

#define PIXEL_ON                       1U
#define UINT_MAX_DIGITS                10U     /* uint32_t มีได้สูงสุด 10 หลัก */
#define DECIMAL_BASE                   10U
#define DECIMAL_BASE_S                 10      /* ฐาน 10 แบบ Signed สำหรับคำนวณทศนิยม (int32_t) */
#define ROUND_HALF                     0.5f
#define FLOAT_DECIMAL_SCALE            10.0f

/* Framebuffer ในหน่วยความจำ: 128x64 พิกเซล เก็บแบบ 1 บิต/พิกเซล รวม 1024 Byte
 * แบ่งเป็น 8 Page แนวตั้ง (Page ละ 8 พิกเซล) ตามโครงสร้าง RAM จริงของ SSD1306
 */
static uint8_t ssd1306_buffer[SSD1306_BUFFER_SIZE];

static void SSD1306_SendCommand(uint8_t cmd);

void SSD1306_Init(void) {
    SoftI2C_Init();

    SSD1306_SendCommand(SSD1306_CMD_DISPLAY_OFF);    /* ปิดจอระหว่างตั้งค่า */
    SSD1306_SendCommand(SSD1306_CMD_SET_CLOCK_DIV);
    SSD1306_SendCommand(SSD1306_CLOCK_DIV_DEFAULT);
    SSD1306_SendCommand(SSD1306_CMD_SET_MULTIPLEX);
    SSD1306_SendCommand(SSD1306_MULTIPLEX_64);
    SSD1306_SendCommand(SSD1306_CMD_SET_OFFSET);
    SSD1306_SendCommand(SSD1306_OFFSET_NONE);
    SSD1306_SendCommand(SSD1306_CMD_START_LINE_0);
    SSD1306_SendCommand(SSD1306_CMD_CHARGE_PUMP);
    SSD1306_SendCommand(SSD1306_CHARGE_PUMP_ON);     /* จำเป็นสำหรับโมดูลที่ไม่มีไฟ VCC แยกสำหรับ Panel */
    SSD1306_SendCommand(SSD1306_CMD_MEMORY_MODE);
    SSD1306_SendCommand(SSD1306_MEMORY_MODE_PAGE);
    SSD1306_SendCommand(SSD1306_CMD_SEG_REMAP);
    SSD1306_SendCommand(SSD1306_CMD_COM_SCAN_DEC);
    SSD1306_SendCommand(SSD1306_CMD_SET_COM_PINS);
    SSD1306_SendCommand(SSD1306_COM_PINS_ALT);
    SSD1306_SendCommand(SSD1306_CMD_SET_CONTRAST);
    SSD1306_SendCommand(SSD1306_CONTRAST_LEVEL);
    SSD1306_SendCommand(SSD1306_CMD_SET_PRECHARGE);
    SSD1306_SendCommand(SSD1306_PRECHARGE_PERIOD);
    SSD1306_SendCommand(SSD1306_CMD_SET_VCOMH);
    SSD1306_SendCommand(SSD1306_VCOMH_LEVEL);
    SSD1306_SendCommand(SSD1306_CMD_RESUME_RAM);
    SSD1306_SendCommand(SSD1306_CMD_NORMAL_DISPLAY);
    SSD1306_SendCommand(SSD1306_CMD_DISPLAY_ON);

    SSD1306_Clear();
    SSD1306_UpdateScreen();
}

static void SSD1306_SendCommand(uint8_t cmd) {
    uint8_t packet[SSD1306_CMD_PACKET_LEN];

    packet[0U] = SSD1306_CTRL_COMMAND;
    packet[1U] = cmd;
    (void)SoftI2C_WriteBytes(SSD1306_I2C_ADDR, packet, (uint16_t)SSD1306_CMD_PACKET_LEN);
}

void SSD1306_Clear(void) {
    uint32_t i;

    for (i = 0U; i < SSD1306_BUFFER_SIZE; i++) {
        ssd1306_buffer[i] = 0x00U;
    }
}

void SSD1306_UpdateScreen(void) {
    uint8_t page;

    for (page = 0U; page < (uint8_t)SSD1306_PAGE_COUNT; page++) {
        uint8_t packet[SSD1306_DATA_PACKET_LEN]; /* [0] = Control Byte (Data), [1..128] = ข้อมูล 1 Page */
        uint16_t i;

        SSD1306_SendCommand((uint8_t)(SSD1306_CMD_PAGE_START + page)); /* เลือก Page ที่จะเขียน (0-7) */
        SSD1306_SendCommand(SSD1306_CMD_COL_LOW_0);                     /* Column Address: Low Nibble = 0 */
        SSD1306_SendCommand(SSD1306_CMD_COL_HIGH_0);                    /* Column Address: High Nibble = 0 */

        packet[0U] = SSD1306_CTRL_DATA;
        for (i = 0U; i < (uint16_t)SSD1306_WIDTH; i++) {
            packet[1U + i] = ssd1306_buffer[((uint32_t)page * SSD1306_WIDTH) + i];
        }
        (void)SoftI2C_WriteBytes(SSD1306_I2C_ADDR, packet, (uint16_t)SSD1306_DATA_PACKET_LEN);
    }
}

void SSD1306_SetPixel(uint16_t x, uint16_t y, uint8_t color) {
    uint32_t index;
    uint8_t bit_mask;

    if ((x < (uint16_t)SSD1306_WIDTH) && (y < (uint16_t)SSD1306_HEIGHT)) {
        index = (uint32_t)x + (((uint32_t)y / SSD1306_PIXELS_PER_PAGE) * SSD1306_WIDTH);
        bit_mask = (uint8_t)(1U << ((uint32_t)y % SSD1306_PIXELS_PER_PAGE));

        if (color != 0U) {
            ssd1306_buffer[index] |= bit_mask;
        } else {
            ssd1306_buffer[index] &= (uint8_t)(~bit_mask);
        }
    } else {
        /* อยู่นอกขอบจอ: ไม่วาด (ป้องกัน Buffer Overflow) */
    }
}

void SSD1306_DrawChar(uint16_t x, uint16_t y, char c) {
    uint8_t ch = (uint8_t)c;
    uint8_t col;

    if ((ch < (uint8_t)FONT_FIRST_CHAR) || (ch > (uint8_t)FONT_LAST_CHAR)) {
        ch = (uint8_t)'?'; /* ตัวอักษรที่ไม่มีในตารางฟอนต์ ใช้ '?' แทน กันไม่ให้วาดเพี้ยน */
    } else {
        /* อยู่ในตารางฟอนต์: ใช้ตามเดิม */
    }

    for (col = 0U; col < (uint8_t)FONT_WIDTH; col++) {
        uint8_t line = font5x7_table[(((uint32_t)ch - FONT_FIRST_CHAR) * FONT_WIDTH) + (uint32_t)col];
        uint8_t row;

        for (row = 0U; row < (uint8_t)FONT_HEIGHT; row++) {
            uint8_t bit = (uint8_t)((line >> row) & 0x01U);
            SSD1306_SetPixel((uint16_t)(x + col), (uint16_t)(y + row), bit);
        }
    }
}

uint16_t SSD1306_DrawString(uint16_t x, uint16_t y, const char *str) {
    uint16_t cursor_x = x;
    const char *p = str; /* ไม่แก้ค่า Parameter โดยตรง ใช้ตัวแปรสำเนาแทน */

    while (*p != '\0') {
        SSD1306_DrawChar(cursor_x, y, *p);
        cursor_x = (uint16_t)(cursor_x + SSD1306_CHAR_ADVANCE);
        p++;
    }
    return cursor_x;
}

/* วาดจำนวนเต็มไม่ติดลบ โดยไม่ใช้ sprintf (Logic เดียวกับ UART2_SendUint แต่วาดลงจอแทนการส่ง UART) */
uint16_t SSD1306_DrawUint(uint16_t x, uint16_t y, uint32_t value) {
    char buf[UINT_MAX_DIGITS];
    uint8_t i = 0U;
    uint16_t cursor_x = x;
    uint32_t v = value;

    if (v == 0U) {
        SSD1306_DrawChar(cursor_x, y, '0');
        cursor_x = (uint16_t)(cursor_x + SSD1306_CHAR_ADVANCE);
    } else {
        while (v > 0U) {
            buf[i] = (char)((v % DECIMAL_BASE) + (uint32_t)'0');
            i++;
            v /= DECIMAL_BASE;
        }

        while (i > 0U) {
            i--;
            SSD1306_DrawChar(cursor_x, y, buf[i]);
            cursor_x = (uint16_t)(cursor_x + SSD1306_CHAR_ADVANCE);
        }
    }
    return cursor_x;
}

/* วาดค่า float ทศนิยม 1 ตำแหน่ง (เช่น "43.5") Logic เดียวกับ UART2_SendFloat1 (ปัดเศษถูกต้องรวมกรณีทด) */
uint16_t SSD1306_DrawFloat1(uint16_t x, uint16_t y, float value) {
    int32_t integer_part;
    int32_t decimal_part;
    uint16_t cursor_x = x;
    float v = value;

    if (v < 0.0f) {
        SSD1306_DrawChar(cursor_x, y, '-');
        cursor_x = (uint16_t)(cursor_x + SSD1306_CHAR_ADVANCE);
        v = -v;
    } else {
        /* ค่าบวก: ไม่ต้องวาดเครื่องหมาย */
    }

    integer_part = (int32_t)v;
    decimal_part = (int32_t)(((v - (float)integer_part) * FLOAT_DECIMAL_SCALE) + ROUND_HALF);

    if (decimal_part >= DECIMAL_BASE_S) {
        decimal_part = 0;
        integer_part++;
    } else {
        /* ไม่มีการทดหลัก */
    }

    cursor_x = SSD1306_DrawUint(cursor_x, y, (uint32_t)integer_part);
    SSD1306_DrawChar(cursor_x, y, '.');
    cursor_x = (uint16_t)(cursor_x + SSD1306_CHAR_ADVANCE);
    cursor_x = SSD1306_DrawUint(cursor_x, y, (uint32_t)decimal_part);
    return cursor_x;
}

void SSD1306_DrawHLine(uint16_t x, uint16_t y, uint16_t length) {
    uint16_t i;

    for (i = 0U; i < length; i++) {
        SSD1306_SetPixel((uint16_t)(x + i), y, PIXEL_ON);
    }
}

void SSD1306_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t i;
    uint16_t bottom = (uint16_t)((y + h) - 1U);
    uint16_t right = (uint16_t)((x + w) - 1U);

    for (i = 0U; i < w; i++) {
        SSD1306_SetPixel((uint16_t)(x + i), y, PIXEL_ON);
        SSD1306_SetPixel((uint16_t)(x + i), bottom, PIXEL_ON);
    }
    for (i = 0U; i < h; i++) {
        SSD1306_SetPixel(x, (uint16_t)(y + i), PIXEL_ON);
        SSD1306_SetPixel(right, (uint16_t)(y + i), PIXEL_ON);
    }
}

void SSD1306_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < w; i++) {
        for (j = 0U; j < h; j++) {
            SSD1306_SetPixel((uint16_t)(x + i), (uint16_t)(y + j), PIXEL_ON);
        }
    }
}
