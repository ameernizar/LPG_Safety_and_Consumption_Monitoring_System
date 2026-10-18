#ifndef RTC_TEST_HAL_H
#define RTC_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
#define HAL_I2C_ERROR_AF 4U
#define I2C_MEMADD_SIZE_8BIT 1U
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t, uint32_t, uint32_t);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
uint32_t HAL_GetTick(void);
#endif
