#include "sh1106.h"
#include "font5x7.h"

static I2C_HandleTypeDef *sh1106_i2c;

static uint8_t SH1106_Buffer[SH1106_WIDTH * SH1106_HEIGHT / 8];

static uint8_t CurrentX = 0;
static uint8_t CurrentY = 0;

static HAL_StatusTypeDef SH1106_WriteCommand(uint8_t command)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = command;

    return HAL_I2C_Master_Transmit(
        sh1106_i2c,
        SH1106_ADDRESS,
        data,
        2,
        100
    );
}

HAL_StatusTypeDef SH1106_Init(I2C_HandleTypeDef *hi2c)
{
    /* Typical 128x64 SH1106 setup; controller identity remains unconfirmed. */
    static const uint8_t commands[] = {
        0xAE,             /* Display off */
        0xD5, 0x80,       /* Display clock */
        0xA8, 0x3F,       /* 64-row multiplex */
        0xD3, 0x00,       /* No vertical offset */
        0x40,             /* Start line 0 */
        0xAD, 0x8B,       /* Internal DC-DC enabled */
        0xA1, 0xC8,       /* Segment and COM scan directions */
        0xDA, 0x12,       /* COM pin configuration */
        0x81, 0x7F,       /* Contrast */
        0xD9, 0x22,       /* Pre-charge */
        0xDB, 0x35,       /* VCOM deselect */
        0xA4, 0xA6        /* RAM display, normal polarity */
    };
    HAL_StatusTypeDef status;

    sh1106_i2c = hi2c;
    if (sh1106_i2c == NULL)
    {
        return HAL_ERROR;
    }
    HAL_Delay(100);
    for (uint32_t i = 0; i < sizeof(commands); i++)
    {
        status = SH1106_WriteCommand(commands[i]);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    /* Clear display RAM before turning the panel on. */
    SH1106_Clear();
    status = SH1106_UpdateScreen();
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(100);
    return SH1106_WriteCommand(0xAF);
}

void SH1106_Clear(void)
{
    memset(SH1106_Buffer, 0, sizeof(SH1106_Buffer));

    CurrentX = 0;
    CurrentY = 0;
}

HAL_StatusTypeDef SH1106_UpdateScreen(void)
{
    uint8_t page;
    uint8_t data[SH1106_WIDTH + 1];

    HAL_StatusTypeDef status;
    if (sh1106_i2c == NULL)
    {
        return HAL_ERROR;
    }
    data[0] = 0x40;

    for (page = 0; page < SH1106_HEIGHT / 8; page++)
    {
        /*
         * SH1106 RAM is 132 pixels wide.
         * Typical 128-pixel modules require a column offset of 2.
         */

        status = SH1106_WriteCommand(0xB0 + page);
        if (status != HAL_OK)
        {
            return status;
        }

        status = SH1106_WriteCommand(0x02);
        if (status != HAL_OK)
        {
            return status;
        }
        status = SH1106_WriteCommand(0x10);
        if (status != HAL_OK)
        {
            return status;
        }

        memcpy(
            &data[1],
            &SH1106_Buffer[SH1106_WIDTH * page],
            SH1106_WIDTH
        );

        status = HAL_I2C_Master_Transmit(
            sh1106_i2c,
            SH1106_ADDRESS,
            data,
            SH1106_WIDTH + 1,
            100
        );
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

void SH1106_SetCursor(uint8_t x, uint8_t y)
{
    CurrentX = x;
    CurrentY = y;
}

void SH1106_WriteChar(char ch)
{
    uint8_t i;
    uint8_t j;

    if (ch < 32 || ch > 90)
    {
        ch = ' ';
    }

    if ((CurrentX + 6) >= SH1106_WIDTH)
    {
        CurrentX = 0;
        CurrentY += 8;
    }

    if ((CurrentY + 7) >= SH1106_HEIGHT)
    {
        return;
    }

    for (i = 0; i < 5; i++)
    {
        uint8_t line = Font5x7[ch - 32][i];

        for (j = 0; j < 7; j++)
        {
            if (line & (1 << j))
            {
                uint16_t index;

                index =
                    CurrentX +
                    i +
                    ((CurrentY + j) / 8) * SH1106_WIDTH;

                SH1106_Buffer[index] |=
                    (1 << ((CurrentY + j) % 8));
            }
        }
    }

    CurrentX += 6;
}

void SH1106_WriteString(const char *str)
{
    while (*str)
    {
        SH1106_WriteChar(*str);
        str++;
    }
}

/* Clipped monochrome primitives; never index outside the 1024-byte buffer. */
void SH1106_Pixel(uint16_t x, uint16_t y, uint8_t on)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;
    uint16_t index = x + (y / 8U) * SH1106_WIDTH;
    uint8_t mask = (uint8_t)(1U << (y % 8U));
    if (on) SH1106_Buffer[index] |= mask;
    else SH1106_Buffer[index] &= (uint8_t)~mask;
}
void SH1106_InvertRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) return;
    uint16_t right = width > SH1106_WIDTH - x ? SH1106_WIDTH : x + width;
    uint16_t bottom = height > SH1106_HEIGHT - y ? SH1106_HEIGHT : y + height;
    for (uint16_t row = y; row < bottom; ++row)
        for (uint16_t col = x; col < right; ++col)
            SH1106_Buffer[col + (row / 8U) * SH1106_WIDTH] ^= (uint8_t)(1U << (row % 8U));
}
