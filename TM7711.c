//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brad Chen

#include "TM7711.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define TM7711_BITS_PER_SAMPLE 24u
#define TM7711_SETTLE_CONVERSIONS 4u
#define TM7711_CLOCK_HIGH_US 1u
#define TM7711_CLOCK_LOW_US 1u
#define TM7711_DATA_SETUP_US 1u

static volatile uint32_t latest_raw = 0u;
static volatile bool new_sample = false;
static volatile uint8_t discard_count = TM7711_SETTLE_CONVERSIONS;
static volatile TM7711_Mode selected_mode = TM7711_DEFAULT_MODE;
static volatile bool driver_ready = false;

static void TM7711_InitCycleCounter(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#if defined(DWT_LAR)
  DWT->LAR = 0xC5ACCE55u;
#endif
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t TM7711_CyclesPerUs(void) { return SystemCoreClock / 1000000u; }

static void TM7711_DelayCycles(uint32_t cycles) {
  const uint32_t start = DWT->CYCCNT;

  while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
    __NOP();
  }
}

static void TM7711_DelayUs(uint32_t us) {
  TM7711_DelayCycles(TM7711_CyclesPerUs() * us);
}

static void TM7711_ClockPulse(void) {
  TM7711_PDSCK_HIGH();
  TM7711_DelayUs(TM7711_CLOCK_HIGH_US);
  TM7711_PDSCK_LOW();
  TM7711_DelayUs(TM7711_CLOCK_LOW_US);
}

static uint32_t TM7711_ReadBurst(TM7711_Mode mode) {
  uint32_t raw = 0u;

  for (uint32_t bit = 0u; bit < TM7711_BITS_PER_SAMPLE; ++bit) {
    TM7711_PDSCK_HIGH();
    TM7711_DelayUs(TM7711_DATA_SETUP_US);
    raw <<= 1u;
    if (TM7711_DOUT_READ() == GPIO_PIN_SET) {
      raw |= 0x1u;
    }
    TM7711_PDSCK_LOW();
    TM7711_DelayUs(TM7711_CLOCK_LOW_US);
  }

  for (uint32_t pulse = TM7711_BITS_PER_SAMPLE; pulse < (uint32_t)mode; ++pulse) {
    TM7711_ClockPulse();
  }

  return raw;
}

void TM7711_Init(void) {
  TM7711_PDSCK_LOW();
  TM7711_InitCycleCounter();

  latest_raw = 0u;
  new_sample = false;
  discard_count = TM7711_SETTLE_CONVERSIONS;
  selected_mode = TM7711_DEFAULT_MODE;
  driver_ready = true;
}

void TM7711_HandleDoutFallingEdge(void) {
  uint32_t primask;
  uint32_t raw;

  if (!driver_ready || TM7711_DOUT_READ() != GPIO_PIN_RESET) {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  raw = TM7711_ReadBurst(selected_mode);
  TM7711_PDSCK_LOW();

  if (discard_count > 0u) {
    --discard_count;
  } else {
    latest_raw = raw;
    new_sample = true;
  }

  if (!primask) {
    __enable_irq();
  }
}

bool TM7711_TryGetLatestRaw(uint32_t* raw) {
  bool has_sample = false;
  uint32_t primask;

  if (raw == NULL) {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  if (new_sample) {
    *raw = latest_raw;
    new_sample = false;
    has_sample = true;
  }

  if (!primask) {
    __enable_irq();
  }

  return has_sample;
}

int32_t TM7711_RawToSigned(uint32_t raw) {
  if ((raw & 0x800000u) != 0u) {
    raw |= 0xFF000000u;
  }

  return (int32_t)raw;
}

int32_t TM7711_ApplyDriftCompensation(int32_t signed_raw, uint32_t tick_ms) {
  const uint32_t elapsed_ms = (tick_ms > TM7711_DRIFT_START_TICK_MS)
                                ? (tick_ms - TM7711_DRIFT_START_TICK_MS)
                                : 0u;
  const float drift_offset =
    TM7711_DRIFT_SLOPE_COUNTS_PER_MS * (float)elapsed_ms;

  return (int32_t)round((double)signed_raw - (double)drift_offset);
}

float ConvertRawToCmH2O(float raw_reading) {
  return CALIBRATION_SLOPE * (raw_reading - RAW_READING_1) + PRESSURE_READING_1;
}

float ConvertCmH2OtoKPa(float cmh2o) { return cmh2o * 0.0980665f; }
