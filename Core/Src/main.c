/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sh1106.h"
#include "safety_state.h"
#include "mq6_monitor.h"
#include "ds3231.h"
#include "oled_ui.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  DS_OK = 0,
  DS_E1_PRESENCE,
  DS_E2_CONVERSION,
  DS_E3_PRESENCE,
  DS_E4_CRC,
  DS_E5_READING,
  DS_E6_INTERNAL,
  DS_PENDING
} DS18B20_Status;

typedef enum
{
  MQ6_PENDING, MQ6_OK, MQ6_INIT_ERROR, MQ6_CAL_ERROR, MQ6_READ_ERROR
} MQ6_Status;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Temporary RESET-triggered image only; normal build must leave this zero. */
#define RTC_RESET_PROVISION 0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
static uint32_t onewire_cycles_per_us;
static uint32_t heartbeat_tick;
static uint8_t onewire_ready;
static uint8_t onewire_timing_fault;
static uint8_t conversion_busy_seen;
static uint8_t conversion_done;
static uint8_t scratchpad_crc_calculated;
static uint8_t scratchpad_crc_received;
static ADC_HandleTypeDef mq6_adc;
static MQ6_Status mq6_status = MQ6_PENDING;
static uint8_t mq6_calibrated;
static uint8_t mq6_batch_saturated;
static MQ6_Monitor mq6_monitor;
static OLED_UI oled_ui;
static DS3231_Reading rtc_reading;
static DS3231_Status rtc_status = DS3231_WAIT;
/* SWD RAM mailbox: all fields are 32-bit words, deliberately zero at boot.
   No RTC writes occur unless ARM then TRIGGER are explicitly received. */
volatile struct {
  uint32_t year, month, date, hour, minute;
  uint32_t command; /* 0, 0x41524D31 ARM1, 0x53455431 SET1 */
  uint32_t state;   /* 0 normal, 1 armed, 2 verifying, 3 success, 4 failed */
  uint32_t error;   /* E1..E6; 0 none */
} rtc_set_request;
static DS3231_Time rtc_target;
static uint32_t rtc_arm_tick;
static DS3231_Status rtc_set_status = DS3231_WAIT;
static uint8_t rtc_set_attempted;

static uint32_t mq6_count, mq6_pin_mv, mq6_ao_mv;
static SafetyStateMachine safety_machine;
static uint16_t safety_led_output;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef OneWire_Init(void);
static uint8_t OneWire_Presence(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* PA0 is reserved for this externally powered, single-device 1-Wire interface.
 * DWT uses HCLK cycles; no timer peripheral or alternate-function pin is used.
 * Timing reference: Analog Devices DS18B20 datasheet, reset/presence sequence.
 */
static uint8_t OneWire_WaitUntil(uint32_t start, uint32_t us)
{
  uint32_t cycles = us * onewire_cycles_per_us;
  /* Guard also terminates if the cycle counter unexpectedly stops. */
  for (uint32_t guard = 0; guard < 100000U; guard++)
  {
    if ((uint32_t)(DWT->CYCCNT - start) >= cycles)
    {
      return 1;
    }
  }
  onewire_timing_fault = 1;
  return 0;
}

static HAL_StatusTypeDef OneWire_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  /* Preload release before enabling open-drain; SET never drives HIGH. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  gpio.Pin = GPIO_PIN_0;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  SystemCoreClockUpdate();
  if (HAL_RCC_GetHCLKFreq() != 72000000U)
  {
    return HAL_ERROR;
  }
  onewire_cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  __DSB();
  __ISB();

  /* Check that this compatible MCU's counter runs without a debugger.
     HAL_Delay is used only for this millisecond sanity check and LED pauses. */
  uint32_t start = DWT->CYCCNT;
  HAL_Delay(2);
  uint32_t elapsed = DWT->CYCCNT - start;
  if (elapsed < onewire_cycles_per_us * 1000U ||
      elapsed > onewire_cycles_per_us * 4000U)
  {
    return HAL_ERROR;
  }
  return HAL_OK;
}

static uint8_t OneWire_Presence(void)
{
  uint8_t detected = 0;
  uint32_t saved_primask;
  uint32_t start;

  /* A disconnected bus pulled HIGH is allowed; a stuck-LOW bus is rejected. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  if (!OneWire_WaitUntil(DWT->CYCCNT, 100U) ||
      HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
  {
    return 0;
  }

  saved_primask = __get_PRIMASK();
  __disable_irq();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  if (!OneWire_WaitUntil(DWT->CYCCNT, 500U))
  {
    goto release;
  }
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  start = DWT->CYCCNT;

  /* The sensor waits 15-60 us, then pulls LOW for 60-240 us.
     Sample HIGH before that window, LOW within its common overlap,
     then HIGH after its latest end. Absolute deadlines avoid delay buildup. */
  if (!OneWire_WaitUntil(start, 10U) ||
      HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
  {
    goto release;
  }
  if (!OneWire_WaitUntil(start, 70U) ||
      HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_RESET)
  {
    goto release;
  }
  if (!OneWire_WaitUntil(start, 310U) ||
      HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
  {
    goto release;
  }
  detected = 1;

release:
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  __set_PRIMASK(saved_primask);
  /* Ensure at least 480 us recovery even on an early failure. */
  HAL_Delay(1);
  return detected;
}
/* All bit slots are LSB-first, open-drain, and bounded by the DWT guard.
   Interrupts are masked for one slot only, preserving the caller's state. */
static uint8_t OneWire_WriteBit(uint8_t bit)
{
  uint32_t saved = __get_PRIMASK();
  uint8_t ok;
  __disable_irq();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  uint32_t start = DWT->CYCCNT;
  ok = OneWire_WaitUntil(start, bit ? 6U : 65U);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  if (ok)
  {
    ok = OneWire_WaitUntil(start, 75U);
  }
  __set_PRIMASK(saved);
  return ok;
}

static uint8_t OneWire_ReadBit(uint8_t *bit)
{
  uint32_t saved = __get_PRIMASK();
  uint8_t ok = 0;
  __disable_irq();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
  uint32_t start = DWT->CYCCNT;
  if (!OneWire_WaitUntil(start, 3U))
  {
    goto release_bit;
  }
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  /* Sample within 15 us of the falling edge, including HAL call overhead. */
  if (!OneWire_WaitUntil(start, 12U))
  {
    goto release_bit;
  }
  *bit = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);
  ok = OneWire_WaitUntil(start, 75U);
release_bit:
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  __set_PRIMASK(saved);
  return ok;
}

static uint8_t OneWire_WriteByte(uint8_t value)
{
  for (uint32_t i = 0; i < 8U; i++)
  {
    if (!OneWire_WriteBit(value & 1U))
    {
      return 0;
    }
    value >>= 1;
  }
  return 1;
}

static uint8_t OneWire_ReadByte(uint8_t *value)
{
  uint8_t bit;
  *value = 0;
  for (uint32_t i = 0; i < 8U; i++)
  {
    if (!OneWire_ReadBit(&bit))
    {
      return 0;
    }
    *value |= (uint8_t)(bit << i);
  }
  return 1;
}

static uint8_t DS18B20_CRC8(const uint8_t *data, uint32_t length)
{
  uint8_t crc = 0;
  for (uint32_t i = 0; i < length; i++)
  {
    uint8_t value = data[i];
    for (uint32_t bit = 0; bit < 8U; bit++)
    {
      uint8_t mix = (crc ^ value) & 1U;
      crc >>= 1;
      if (mix)
      {
        crc ^= 0x8CU; /* Reflected x^8 + x^5 + x^4 + 1. */
      }
      value >>= 1;
    }
  }
  return crc;
}

static DS18B20_Status DS18B20_Decode(const uint8_t scratchpad[9], int16_t *tenths)
{
  /* The configuration's fixed bits also reject an all-zero stuck-bus frame,
     whose CRC alone would otherwise pass. An all-FF frame fails CRC. */
  scratchpad_crc_calculated = DS18B20_CRC8(scratchpad, 8U);
  scratchpad_crc_received = scratchpad[8];
  if (scratchpad_crc_calculated != scratchpad_crc_received)
  {
    return DS_E4_CRC;
  }
  if ((scratchpad[4] & 0x9FU) != 0x1FU)
  {
    return DS_E5_READING;
  }
  uint16_t word = (uint16_t)scratchpad[0] | ((uint16_t)scratchpad[1] << 8);
  uint32_t resolution = (scratchpad[4] >> 5) & 3U;
  /* Undefined low bits must be discarded at 9/10/11-bit resolution. */
  word &= (uint16_t)(0xFFFFU << (3U - resolution));
  int32_t raw = (word & 0x8000U) ? (int32_t)word - 65536 : (int32_t)word;
  if (raw < -55 * 16 || raw > 125 * 16)
  {
    return DS_E5_READING;
  }
  /* Without evidence of BUSY, 85 C could still be the power-on default. */
  if (raw == 85 * 16 && !conversion_busy_seen)
  {
    return DS_E5_READING;
  }
  /* Round to one decimal place, symmetrically for negative temperatures. */
  *tenths = (int16_t)((raw * 10 + (raw < 0 ? -8 : 8)) / 16);
  return DS_OK;
}

/* GPIO adapter is separate from pure state evaluation and OLED rendering. */
static void SafetyLED_Apply(uint8_t logical_leds)
{
  const uint16_t all_leds = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
  uint16_t output = 0;
  /* At most one output, even if a future caller supplies multiple bits. */
  if (logical_leds & SAFETY_LED_RED) output = GPIO_PIN_5;
  else if (logical_leds & SAFETY_LED_YELLOW) output = GPIO_PIN_4;
  else if (logical_leds & SAFETY_LED_GREEN) output = GPIO_PIN_3;
  if (output != safety_led_output)
  {
    HAL_GPIO_WritePin(GPIOA, (uint16_t)(all_leds & ~output), GPIO_PIN_RESET);
    if (output != 0U) HAL_GPIO_WritePin(GPIOA, output, GPIO_PIN_SET);
    safety_led_output = output;
  }
}

static void Safety_Service(void)
{
  uint32_t now = HAL_GetTick();
  (void)SafetyState_Update(&safety_machine, now);
  SafetyLED_Apply(SafetyState_Leds(&safety_machine, now));
}

static void Heartbeat_Service(void)
{
  Safety_Service();
  uint32_t now = HAL_GetTick();
  if ((uint32_t)(now - heartbeat_tick) >= 500U)
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    heartbeat_tick = now;
  }
}

static DS18B20_Status DS18B20_ReadTemperature(int16_t *tenths)
{
  uint8_t ready;
  uint8_t scratchpad[9];
  conversion_busy_seen = 0;
  conversion_done = 0;
  scratchpad_crc_calculated = 0;
  scratchpad_crc_received = 0;
  onewire_timing_fault = 0;
  if (!onewire_ready)
  {
    return DS_E6_INTERNAL;
  }
  if (!OneWire_Presence())
  {
    return onewire_timing_fault ? DS_E6_INTERNAL : DS_E1_PRESENCE;
  }
  if (!OneWire_WriteByte(0xCC) || !OneWire_WriteByte(0x44))
  {
    return DS_E6_INTERNAL;
  }
  uint32_t conversion_start = HAL_GetTick();
  /* Record BUSY as evidence, but do not fail on an immediate HIGH.
     Externally powered probe: line stays released during the fixed wait. */
  if (!OneWire_ReadBit(&ready))
  {
    return DS_E6_INTERNAL;
  }
  conversion_busy_seen = !ready;
  while ((uint32_t)(HAL_GetTick() - conversion_start) < 800U)
  {
    Heartbeat_Service();
    HAL_Delay(1);
  }
  if (!OneWire_ReadBit(&ready))
  {
    return DS_E6_INTERNAL;
  }
  conversion_done = ready;
  if (!ready)
  {
    return DS_E2_CONVERSION;
  }
  if (!OneWire_Presence())
  {
    return onewire_timing_fault ? DS_E6_INTERNAL : DS_E3_PRESENCE;
  }
  if (!OneWire_WriteByte(0xCC) || !OneWire_WriteByte(0xBE))
  {
    return DS_E6_INTERNAL;
  }
  for (uint32_t i = 0; i < sizeof(scratchpad); i++)
  {
    if (!OneWire_ReadByte(&scratchpad[i]))
    {
      return DS_E6_INTERNAL;
    }
  }
  return DS18B20_Decode(scratchpad, tenths);
}

/* ADC1 is dedicated to MQ-6 AO through the external 10k/10k divider.
   Kept in USER CODE so failures can be displayed instead of halting firmware. */
static void MQ6_Init(void)
{
  RCC_PeriphCLKInitTypeDef clock = {0};
  GPIO_InitTypeDef gpio = {0};
  ADC_ChannelConfTypeDef channel = {0};
  mq6_status = MQ6_INIT_ERROR;
  mq6_calibrated = 0;
  clock.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  clock.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK ||
      HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC) != 12000000U)
  {
    return;
  }
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC1_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_2;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  mq6_adc.Instance = ADC1;
  /* STM32F1 ADC resolution is fixed at 12 bits. */
  mq6_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  mq6_adc.Init.ContinuousConvMode = DISABLE;
  mq6_adc.Init.DiscontinuousConvMode = DISABLE;
  mq6_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  mq6_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  mq6_adc.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&mq6_adc) != HAL_OK)
  {
    return;
  }
  channel.Channel = ADC_CHANNEL_2;
  channel.Rank = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&mq6_adc, &channel) != HAL_OK)
  {
    return;
  }
  /* HAL performs the F1 ADC power-up/calibration prerequisite delays. */
  mq6_status = MQ6_CAL_ERROR;
  if (HAL_ADCEx_Calibration_Start(&mq6_adc) != HAL_OK)
  {
    return;
  }
  mq6_calibrated = 1;
  mq6_status = MQ6_PENDING;
}

static void MQ6_Sample(void)
{
  uint32_t sum = 0;
  if (!mq6_calibrated)
  {
    return; /* Preserve the initialization/calibration error. */
  }
  mq6_status = MQ6_READ_ERROR;
  mq6_batch_saturated = 0;
  for (uint32_t i = 0; i < 16U; i++)
  {
    HAL_StatusTypeDef conversion = HAL_ADC_Start(&mq6_adc);
    if (conversion == HAL_OK)
    {
      conversion = HAL_ADC_PollForConversion(&mq6_adc, 10U);
    }
    uint32_t sample = 0;
    if (conversion == HAL_OK)
    {
      sample = HAL_ADC_GetValue(&mq6_adc);
    }
    HAL_StatusTypeDef stopped = HAL_ADC_Stop(&mq6_adc);
    if (conversion != HAL_OK || stopped != HAL_OK || sample > 4095U)
    {
      return; /* Never publish a partial average or retain a valid flag. */
    }
    if (sample == 0U) mq6_batch_saturated |= 1U;
    if (sample == 4095U) mq6_batch_saturated |= 2U;
    sum += sample;
  }
  mq6_count = (sum + 8U) / 16U;
  /* Nominal VDDA=3.3V and equal divider resistors: estimates, not calibration. */
  mq6_pin_mv = (mq6_count * 3300U + 2047U) / 4095U;
  mq6_ao_mv = (mq6_count * 6600U + 2047U) / 4095U;
  mq6_status = MQ6_OK;
}

/* Calendar validation is repeated by the driver before any register write. */
static uint8_t RTC_LoadTarget(void)
{
  uint32_t year = rtc_set_request.year, month = rtc_set_request.month;
  uint32_t date = rtc_set_request.date, hour = rtc_set_request.hour;
  uint32_t minute = rtc_set_request.minute;
  if (year < 2000U || year > 2199U || month < 1U || month > 12U ||
      date < 1U || date > 31U || hour > 23U || minute > 59U) return 0;
  uint8_t r[7] = {0};
  r[1] = (minute / 10U) * 16U + minute % 10U;
  r[2] = (hour / 10U) * 16U + hour % 10U;
  r[3] = 1;
  r[4] = (date / 10U) * 16U + date % 10U;
  r[5] = (month / 10U) * 16U + month % 10U + (year >= 2100U ? 128U : 0U);
  r[6] = ((year % 100U) / 10U) * 16U + year % 10U;
  if (!DS3231_Decode(r, &rtc_target)) return 0;
  /* Gregorian weekday, Monday=1..Sunday=7. */
  static const unsigned offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  uint32_t y = year - (month < 3U);
  unsigned sunday_zero = (y + y/4U - y/100U + y/400U + offsets[month-1U] + date) % 7U;
  rtc_target.day = sunday_zero == 0U ? 7U : sunday_zero;
  return 1;
}
static void RTC_SetResult(DS3231_Status result)
{
  rtc_set_status = result;
  if (result == DS3231_VERIFYING) { rtc_set_request.state = 2; return; }
  rtc_set_request.state = result == DS3231_OK ? 3 : 4;
  switch (result)
  {
  case DS3231_OK: rtc_set_request.error = 0; break;
  case DS3231_NO_ACK: case DS3231_IO_ERROR: rtc_set_request.error = 1; break;
  case DS3231_WRITE_ERROR: rtc_set_request.error = 2; break;
  case DS3231_OSF_ERROR: rtc_set_request.error = 4; break;
  case DS3231_INVALID_DATA: rtc_set_request.error = 5; break;
  default: rtc_set_request.error = 3; break;
  }
}
static void RTC_SetService(void)
{
  uint32_t command = rtc_set_request.command;
  rtc_set_request.command = 0; /* Consume each command; never auto-retry. */
  if (rtc_set_request.state == 2U)
  {
    RTC_SetResult(DS3231_ServiceSet(&hi2c1));
    return;
  }
  if (rtc_set_attempted) return; /* At most one write attempt per boot. */
  if (command == 0x41524D31U)
  {
    rtc_set_request.error = 0;
    if (!RTC_LoadTarget()) { RTC_SetResult(DS3231_INVALID_DATA); return; }
    rtc_arm_tick = HAL_GetTick();
    rtc_set_request.state = 1;
    rtc_set_status = DS3231_WAIT;
  }
  else if (command == 0x53455431U && rtc_set_request.state == 1U &&
           (uint32_t)(HAL_GetTick() - rtc_arm_tick) < 60000U)
  {
    rtc_set_attempted = 1;
    RTC_SetResult(DS3231_SetUnsetClock(&hi2c1, &rtc_target));
  }
  else if (command != 0U || (rtc_set_request.state == 1U &&
           (uint32_t)(HAL_GetTick() - rtc_arm_tick) >= 60000U))
  {
    rtc_set_request.state = 4;
    rtc_set_request.error = 6;
  }
}

static void OLED_ShowTemperature(DS18B20_Status status, int16_t tenths)
{
  const OLED_UI_Data view = {
    .temperature_tenths = tenths, .temperature_status = status,
    .flame_high = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET,
    .crc_calculated = scratchpad_crc_calculated, .crc_received = scratchpad_crc_received,
    .conversion_busy = conversion_busy_seen, .conversion_done = conversion_done,
    .mq6 = &mq6_monitor, .adc_status = mq6_status,
    .pin_mv = mq6_pin_mv, .ao_mv = mq6_ao_mv,
    .rtc_status = rtc_status, .rtc_set_status = rtc_set_status, .rtc = &rtc_reading,
    .rtc_set_state = rtc_set_request.state, .rtc_set_error = rtc_set_request.error,
    .mode = safety_machine.mode, .state = safety_machine.state
  };
  if (OLED_UI_Render(&oled_ui, &view, HAL_GetTick()) != HAL_OK)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    Error_Handler(); /* Preserve existing OLED transfer failure behavior. */
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  if (SH1106_Init(&hi2c1) != HAL_OK)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    Error_Handler();
  }
  MQ6_Init(); /* Calibrate once; report failures on OLED. */
  MQ6_MonitorInit(&mq6_monitor, HAL_GetTick());
  onewire_ready = (OneWire_Init() == HAL_OK);
  heartbeat_tick = HAL_GetTick();
  SafetyState_Init(&safety_machine, heartbeat_tick); /* DEMO by default. */
  safety_led_output = 0;
  Safety_Service();
  /* Commands must be supplied after boot. Never restore an old request. */
  rtc_set_request.command = 0;
  rtc_set_request.state = 0;
  rtc_set_request.error = 0;
#if RTC_RESET_PROVISION
  /* Explicit user target: 18-10-2026 09:43:00 IST; Sunday=7.
     Hold RESET through programming and release at target. Startup adds latency. */
  const DS3231_Time reset_target = {
    .year = 2026, .month = 10, .date = 18, .day = 7,
    .hour = 9, .minute = 43, .second = 0
  };
  rtc_set_attempted = 1;
  RTC_SetResult(DS3231_SetUnsetClock(&hi2c1, &reset_target));
#endif
  /* No reading is valid before a completed conversion and CRC validation. */
  OLED_ShowTemperature(onewire_ready ? DS_PENDING : DS_E6_INTERNAL, 0);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t cycle_start = HAL_GetTick();
    int16_t temperature_tenths = 0;
    DS18B20_Status status = DS18B20_ReadTemperature(&temperature_tenths);
    Heartbeat_Service();
    MQ6_Sample(); /* After 1-Wire activity, with interrupts enabled. */
    MQ6_MonitorUpdate(&mq6_monitor, HAL_GetTick(), mq6_status == MQ6_OK,
                       (uint16_t)mq6_count, mq6_batch_saturated);
    /* Serialized with OLED access, outside 1-Wire slots; bounded I2C calls. */
    RTC_SetService();
    rtc_status = DS3231_Read(&hi2c1, &rtc_reading);
    Heartbeat_Service();
    OLED_ShowTemperature(status, temperature_tenths);
    while ((uint32_t)(HAL_GetTick() - cycle_start) < 1000U)
    {
      Heartbeat_Service();
      HAL_Delay(1);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* Active-high demo LEDs. Preload LOW before enabling output mode. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA1: flame module digital output; raw level, no assumed polarity.
     PA0 remains 1-Wire; PB8/PB9 remain reserved for the future HX711. */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
