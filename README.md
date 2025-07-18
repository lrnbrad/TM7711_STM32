# TM7711 - A Hardware-Agnostic ADC Driver

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This Library is adapted from the
original [hx710b_non_blocking_f767zi](https://github.com/ufnalski/hx710b_non_blocking_f767zi.git),
check it out for more details on the original implementation.

This is a driver for the TM7711, a 24-bit analog-to-digital converter, commonly used for weigh scales and pressure
sensors. This library was originally developed on an STM32L4 using the HAL library.

The driver utilizes SPI communication and is optimized for DMA usage to minimize CPU load, making it suitable for
real-time applications.

## Features

* **Multiple Operating Modes:** Supports three main operating modes defined by the TM7711 datasheet:
    * Differential Input, 10Hz Output Rate (`TM7711_DIFF_10_HZ`)
    * Temperature Sensor, 40Hz Output Rate (`TM7711_TEMP_40_HZ`)
    * Differential Input, 40Hz Output Rate (`TM7711_DIFF_40_HZ`)
* **DMA-based Reading:** Uses DMA for SPI communication (`TM7711_Read_Raw_DMA`) to offload data transfer from the CPU.
* **Calibration Utilities:** Includes functions to convert the raw ADC value into a signed float and then into physical
  units (cmH₂O and kPa) using a two-point linear calibration.
* **Portable Design:** While built on STM32 HAL, all platform-specific definitions are centralized in `TM7711.h` for
  easy porting.

## Hardware & SPI Configuration

To use this library, your MCU's SPI peripheral must be configured with the following settings, as required by the
TM7711:

* **Data Size:** 8 bits
* **First Bit:** MSB First
* **Baud Rate:** Refer to datasheet and your clock configuration
* **Clock Polarity (CPOL):** Low (0)
* **Clock Phase (CPHA):** 2 Edge (1) - Data is sampled on the falling edge of the clock.
* **Chip Select (NSS/CS):** Software managed.
* 

**DMA Configuration:**
* - **SPIx_RX**: Peripheral to Memory, Data Width: Byte, Memory Increment: Enable
* - **SPIx_TX**: Memory to Peripheral, Data Width: Byte, Memory Increment: Enable
* - **Priority**: Medium or High recommended

## How to Use

### 1. Configuration (`TM7711.h`)

Before compiling, you need to configure the library by editing the macros in `TM7711.h`.

**a. Select Operating Mode:**
Change one of the following macros to choose the desired mode. Only one mode can be active at a time.

```c
#define TM7711_DIFF_10_HZ 1 
#define TM7711_TEMP_40_HZ 0
#defone TM7711_DIFF_40_HZ 0
```

**b. Platform-Specific Definitions:**
This is the most critical step for porting.
Modify these macros to match your MCU's hardware and HAL/driver functions.
see `TM7711.h` for detailed comments on each macro.

```c
// Your SPI peripheral instance and handle
#define SPI_INSTANCE SPI1
extern SPI_HandleTypeDef hspi1;
#define SPI_HANDLE (&hspi1)

// A flag that will be set to 1 in your SPI receive complete callback/ISR
extern volatile uint8_t SPI_Comp_Flag;
#define SPI_COMP_FLAG SPI_Comp_Flag

// A function/macro to read the state of the DOUT pin (MISO)
// The TM7711 pulls this pin low when data is ready.
#define DOUT_STATUS HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6)
```

**c. Calibration:**
You must perform your own two-point calibration to get accurate pressure readings. Apply two known pressures
(e.g., 1.0 cmH₂O and 13.5 cmH₂O), record the raw ADC values, and update these macros.

```c
#define RAW_READING_1 1957000.0f
#define PRESSURE_READING_1 1.0f  // cmH20
#define RAW_READING_2 2614400.0f
#define PRESSURE_READING_2 13.5f  // cmH20
```

### 2. Integration into Your Code

**a. Initialization**
In your main application file, initialize the SPI peripheral and the GPIO for the CS pin according to the requirements
listed above.

**b. Implement SPI Callback**
You need to implement the SPI "Transmit and Receive Complete" callback for your platform. Inside this callback, set the
SPI_Comp_Flag to 1. For STM32 HAL, this is in `TM7711.c`:

```c
// This function is defined as weak in the HAL, so you can redefine it.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI_INSTANCE) {
        SPI_COMP_FLAG = 1;
    }
}
```

**c. Reading Data**
To read data from the TM7711, before while(1), call the `TM7711_Read_Raw_DMA` function. This function will initiate a
DMA transfer to read the raw ADC value, then call `TM7711_Read_Polling` to update the raw value and convert it to
physical units at desired place.

```c
#include "TM7711.h"
#include <stdio.h>

int main(void) {
    // ... MCU and peripheral initialization ...
    TM7711_Read_Raw_DMA();
    
    while (1) {
        uint32_t raw_adc = ConvertSPItoRawReading();
        float signed_float_adc = ConvertRawReadingToSignedFloat(raw_adc);
        float pressure_cmh2o = ConvertRawToCmH2O(signed_float_adc);
        float pressure_kpa = ConvertCmH2OtoKPa(pressure_cmh2o);

        printf("Raw: %lu, Pressure: %.2f cmH2O (%.3f kPa)\r\n",
                raw_adc, pressure_cmh2o, pressure_kpa);
            
        // Note: TM7711_Read_Polling() clears the flag before starting a new transfer.
        // Before you can print floating point, make sure you set the flag in the cmake file like
        // set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")
        // in gcc-arm-none-eabi.cmake or somewhere else in your build system.
    }
}
```

## License

This project is licensed under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue.
