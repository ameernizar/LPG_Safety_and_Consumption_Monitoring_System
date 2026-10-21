#ifndef HX_TEST_HAL
#define HX_TEST_HAL
#include <stdint.h>
typedef struct { uint32_t CTRL, CYCCNT; } DWT_Type;
typedef struct { uint32_t DEMCR; } CoreDebug_Type;
DWT_Type *TestDWT(void);
extern CoreDebug_Type test_core;
#define DWT (TestDWT())
#define CoreDebug (&test_core)
#define DWT_CTRL_CYCCNTENA_Msk 1U
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define GPIOB ((void *)1)
#define GPIO_PIN_8 256U
#define GPIO_PIN_9 512U
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_HIGH 3U
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(void *, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(void *, uint16_t);
uint32_t HAL_RCC_GetHCLKFreq(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
#endif
