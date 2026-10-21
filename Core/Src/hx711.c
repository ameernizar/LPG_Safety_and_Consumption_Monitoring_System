#include "hx711.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* AVIA HX711: 24 MSB-first bits + one clock selects channel A, gain 128.
 * 1 us minimum high/low; interrupts masked only while SCK is HIGH, preventing
 * an ISR from extending HIGH beyond the 60 us power-down threshold.
 * DWT is shared with DS18B20 and is NEVER reset. No timer resource consumed. */
static uint8_t DelayUs(const HX711 *h)
{
    uint32_t start = DWT->CYCCNT;
    for (unsigned guard = 0; guard < 64U; ++guard)
        if ((uint32_t)(DWT->CYCCNT - start) >= h->cycles_per_us) return 1;
    return 0; /* Counter stopped: bounded failure, including inside a pulse. */
}
static void Fail(HX711 *h, HX711_Error error, uint32_t now)
{
    h->state = HX711_ERROR; h->error = error; h->valid = 0;
    h->count = h->index = h->stable = h->synced = 0;
    h->start_tick = now;
}
void HX711_Init(HX711 *h, uint32_t now)
{
    memset(h, 0, sizeof(*h));
    h->start_tick = h->last_tick = h->poll_tick = now;
    h->state = HX711_WAITING;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_9; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_8; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
    h->cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    if (HAL_RCC_GetHCLKFreq() != 72000000U || !DelayUs(h))
        Fail(h, HX711_TIMING, now);
}
void HX711_Service(HX711 *h, uint32_t now)
{
    if (h->error == HX711_TIMING || (uint32_t)(now-h->poll_tick) < 1U) return;
    h->poll_tick = now;
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET) {
        if ((uint32_t)(now-h->last_tick) >= HX711_TIMEOUT_MS)
            Fail(h, HX711_NO_DATA, now);
        return;
    }
    uint32_t bits = 0;
    if (!DelayUs(h)) { Fail(h,HX711_TIMING,now); return; }
    for (unsigned i = 0; i < 25U; ++i) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
        uint8_t timed = DelayUs(h);
        if (i < 24U) bits = (bits << 1) | (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8) == GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        __set_PRIMASK(mask);
        if (!timed || !DelayUs(h)) { Fail(h,HX711_TIMING,now); return; }
    }
    if (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_8) != GPIO_PIN_SET) {
        Fail(h,HX711_PROTOCOL,now); return;
    }
    h->last_tick = now;
    /* First frame may have unknown previous gain. Drain subsequent frames
     * during settling instead of retaining an early conversion until 500 ms. */
    if (!h->synced) {
        h->synced = 1; h->start_tick = now;
        h->state = HX711_WAITING; h->error = HX711_NO_ERROR; return;
    }
    if ((uint32_t)(now-h->start_tick) < HX711_SETTLE_MS) return;
    if (bits == 0x7FFFFFU || bits == 0x800000U) {
        Fail(h,HX711_SATURATED,now); return;
    }
    /* Subtraction avoids implementation-defined unsigned-to-signed overflow. */
    h->raw = (bits & 0x800000U) ? (int32_t)bits - 16777216 : (int32_t)bits;
    h->samples[h->index] = h->raw;
    h->index = (uint8_t)((h->index+1U)%HX711_WINDOW);
    if (h->count < HX711_WINDOW) ++h->count;
    int32_t min = h->samples[0], max = min;
    for (unsigned i=1; i<h->count; ++i) {
        if (h->samples[i] < min) min = h->samples[i];
        if (h->samples[i] > max) max = h->samples[i];
    }
    h->range = (uint32_t)(max-min);
    h->stable = h->count == HX711_WINDOW && h->range <= HX711_STABLE_RANGE;
    h->valid = 1; h->state = HX711_READY; h->error = HX711_NO_ERROR;
}
