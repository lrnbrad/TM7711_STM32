# TM7711 - STM32 HAL Driver

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This library is inspired by [hx710b_non_blocking_f767zi](https://github.com/ufnalski/hx710b_non_blocking_f767zi.git),
and has since been reshaped for the current STM32L432 TM7711 test project.

The current implementation is a lightweight TM7711 driver for STM32 HAL that:

* reads the 24-bit TM7711 result on `DOUT` falling edges
* stores the latest converted sample without blocking the main loop
* exposes helpers for signed conversion, linear drift compensation, and pressure calibration
* is configured mainly through macros in `TM7711.h`

## Features

* **Supported TM7711 modes:** Differential `10Hz`, temperature `40Hz`, and differential `40Hz`
* **Non-blocking sample acquisition:** Samples are latched from the EXTI callback and consumed later in the main loop
* **Signed conversion helper:** Converts the raw 24-bit TM7711 word into a signed `int32_t`
* **Linear drift compensation:** Applies a fitted linear drift correction using project-defined tick and fit macros
* **Pressure conversion helpers:** Converts calibrated raw ADC counts into `cmH2O` and `kPa`

## Hardware Configuration

This driver does not use the MCU SPI peripheral. It clocks the TM7711 with GPIO and reads data from the `DOUT` pin
when the converter signals that a new sample is ready.

Current project pin mapping in [`TM7711.h`](./TM7711.h):

* **DOUT:** `PA5`
* **PD_SCK:** `PA6`

Requirements:

* Configure `DOUT` as a GPIO input with EXTI on the falling edge
* Configure `PD_SCK` as a GPIO output
* Route the EXTI callback to `TM7711_HandleDoutFallingEdge()`
* Provide a valid tick source through `TM7711_GET_TICK_MS()`

## CubeMX GPIO Setup

For the current STM32L432 project, the CubeMX GPIO setup is:

* `PA5`:
  `GPIO_EXTI5`, `External Interrupt Mode with Falling edge trigger detection`
* `PA6`:
  `GPIO_Output`, `Push-Pull`, `No pull-up and no pull-down`, `Very High` speed
* NVIC:
  enable `EXTI line[9:5] interrupts`

The generated initialization should end up equivalent to:

```c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

GPIO_InitStruct.Pin = GPIO_PIN_6;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_5;
GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
```

After CubeMX generates the project, the EXTI callback still needs to be connected manually:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TM7711_DOUT_PIN) {
        TM7711_HandleDoutFallingEdge();
    }
}
```

## How To Use

### 1. Configuration (`TM7711.h`)

Before compiling, review the macros in [`TM7711.h`](./TM7711.h).

**a. Select the TM7711 mode**

The current default is:

```c
#define TM7711_DEFAULT_MODE TM7711_MODE_DIFF_40HZ
```

Available values:

```c
TM7711_MODE_DIFF_10HZ
TM7711_MODE_TEMP_40HZ
TM7711_MODE_DIFF_40HZ
```

**b. Platform-specific GPIO and tick macros**

These macros are the main porting layer:

```c
#define TM7711_DOUT_READ() HAL_GPIO_ReadPin(TM7711_DOUT_PORT, TM7711_DOUT_PIN)
#define TM7711_PDSCK_HIGH() HAL_GPIO_WritePin(TM7711_PDSCK_PORT, TM7711_PDSCK_PIN, GPIO_PIN_SET)
#define TM7711_PDSCK_LOW()  HAL_GPIO_WritePin(TM7711_PDSCK_PORT, TM7711_PDSCK_PIN, GPIO_PIN_RESET)

#ifndef TM7711_GET_TICK_MS
#define TM7711_GET_TICK_MS() HAL_GetTick()
#endif
```

If you move the driver to another MCU or HAL, update these macros first.

**c. Drift compensation fit**

The current project uses a fitted linear drift term in ADC counts per millisecond:

```c
#define TM7711_DRIFT_START_TICK_MS 3162u
#define TM7711_DRIFT_SLOPE_COUNTS_PER_MS 0.0118858412994288f
```

This means the compensation helper subtracts:

```text
drift_offset = slope * (tick_ms - start_tick_ms)
```

Set `TM7711_DRIFT_START_TICK_MS` to the same time origin used when the line was fitted.

**d. Pressure calibration**

Pressure conversion uses a two-point linear calibration:

```c
#define RAW_READING_1 1957000.0f
#define PRESSURE_READING_1 1.0f
#define RAW_READING_2 2614400.0f
#define PRESSURE_READING_2 13.5f
```

Update these values with your own measured calibration points.

### 2. Integration Into Your Code

**a. Initialization**

Initialize the GPIOs and the rest of the MCU peripherals, then call:

```c
TM7711_Init();
```

**b. EXTI callback**

Forward the TM7711 `DOUT` falling edge into the driver:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TM7711_DOUT_PIN) {
        TM7711_HandleDoutFallingEdge();
    }
}
```

**c. `app.c` sample**

The driver stores the newest unread sample internally. A minimal application flow can look like this:

```c
#include "app.h"
#include "../Lib/TM7711_STM32/TM7711.h"

#include <inttypes.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;

volatile uint8_t tim6_exp = 0u;
char s[100] = {0};

void app(void) {
    int len = 0;
    uint32_t raw_adc = 0u;
    int32_t signed_adc = 0;
    int32_t corrected_adc = 0;
    uint32_t ts_ms = 0u;

    if (!TM7711_TryGetLatestRaw(&raw_adc)) {
        return;
    }

    // You can use the raw ADC value as well, we found it useful to eilminate
    // the compensation within a short time (~10min)
    ts_ms = TM7711_GET_TICK_MS();
    signed_adc = TM7711_RawToSigned(raw_adc);
    
    corrected_adc = TM7711_ApplyDriftCompensation(signed_adc, ts_ms);

    len = snprintf(s, sizeof(s), "%" PRIu32 ",%" PRId32 "\r\n", ts_ms, corrected_adc);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)len, HAL_MAX_DELAY);
    }
}
```

And in `main()`:

```c
MX_GPIO_Init();
MX_USART2_UART_Init();
MX_TIM6_Init();

TM7711_Init();
HAL_TIM_Base_Start_IT(&htim6);

while (1) {
    app();
}
```

Current helper functions:

* `TM7711_TryGetLatestRaw()` returns each fresh raw sample once
* `TM7711_RawToSigned()` sign-extends the 24-bit TM7711 output
* `TM7711_ApplyDriftCompensation()` subtracts the fitted linear drift term
* `ConvertRawToCmH2O()` and `ConvertCmH2OtoKPa()` convert calibrated pressure values

## License

The TM7711 driver library in this folder is licensed under the MIT License.

## Contributing

Contributions are welcome. Keep `TM7711.c`, `TM7711.h`, and this README aligned when changing the sampling,
compensation, or integration flow.
