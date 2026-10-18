#ifndef INC_SH1106_H_
#define INC_SH1106_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

#define SH1106_WIDTH    128
#define SH1106_HEIGHT   64

/* HAL expects the 7-bit I2C address shifted left by one */
#define SH1106_ADDRESS  (0x3C << 1)

HAL_StatusTypeDef SH1106_Init(I2C_HandleTypeDef *hi2c);
void SH1106_Clear(void);
HAL_StatusTypeDef SH1106_UpdateScreen(void);

void SH1106_SetCursor(uint8_t x, uint8_t y);
void SH1106_WriteChar(char ch);
void SH1106_WriteString(const char *str);

void SH1106_Pixel(uint16_t x, uint16_t y, uint8_t on);
void SH1106_InvertRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

#endif
