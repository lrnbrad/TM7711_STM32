//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Brad Chen

#ifndef TM7711_H
#define TM7711_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx.h"
#include "stm32l4xx_hal_gpio.h"

typedef enum {
  TM7711_MODE_DIFF_10HZ = 25,
  TM7711_MODE_TEMP_40HZ = 26,
  TM7711_MODE_DIFF_40HZ = 27,
} TM7711_Mode;

#define TM7711_DEFAULT_MODE TM7711_MODE_DIFF_40HZ

#define TM7711_DOUT_PORT GPIOA
#define TM7711_DOUT_PIN GPIO_PIN_5
#define TM7711_DOUT_READ() HAL_GPIO_ReadPin(TM7711_DOUT_PORT, TM7711_DOUT_PIN)

#define TM7711_PDSCK_PORT GPIOA
#define TM7711_PDSCK_PIN GPIO_PIN_6
#define TM7711_PDSCK_HIGH() \
  HAL_GPIO_WritePin(TM7711_PDSCK_PORT, TM7711_PDSCK_PIN, GPIO_PIN_SET)
#define TM7711_PDSCK_LOW() \
  HAL_GPIO_WritePin(TM7711_PDSCK_PORT, TM7711_PDSCK_PIN, GPIO_PIN_RESET)

#ifndef TM7711_GET_TICK_MS
#define TM7711_GET_TICK_MS() HAL_GetTick()
#endif

// Linear drift fit in ADC counts per millisecond.
#define TM7711_DRIFT_START_TICK_MS 3162u
#define TM7711_DRIFT_SLOPE_COUNTS_PER_MS 0.0118858412994288f

// Calibration factor
#define RAW_READING_1 1957000.0f
#define PRESSURE_READING_1 1.0f  // cmH20
#define RAW_READING_2 2614400.0f
#define PRESSURE_READING_2 13.5f  // cmH20
#define CALIBRATION_SLOPE \
  (PRESSURE_READING_2 - PRESSURE_READING_1) / (RAW_READING_2 - RAW_READING_1)

void TM7711_Init(void);
void TM7711_HandleDoutFallingEdge(void);
bool TM7711_TryGetLatestRaw(uint32_t* raw);
int32_t TM7711_RawToSigned(uint32_t raw);
int32_t TM7711_ApplyDriftCompensation(int32_t signed_raw, uint32_t tick_ms);

float ConvertRawToCmH2O(float raw_reading);
float ConvertCmH2OtoKPa(float cmh2o);

#endif  // TM7711_H
