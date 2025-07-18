//
// Created by brad on 7/17/25.
//

#ifndef TM7711_H
#define TM7711_H
#include <stdint.h>

// Make this import to your MCU library
#include "stm32l4xx.h"
#include "stm32l4xx_hal_spi.h"

#define TM7711_DIFF_10_HZ 0
#define TM7711_TEMP_40_HZ 0
#define TM7711_DIFF_40_HZ 1

/**
 * Change this to your SPI instance and flag
 * SPI parameter settings:
 * - Data Size: 8 bits
 * - First Bit: MSB
 * - Baud Rate: based on your clock speed, and must meet the requirements of TM7711
 * - CPOL: Low (Clock Polarity = 0, idle state is low)
 * - CPHA: 2 Edge (Clock Phase = 1, data sampled on falling edge)
 * - NSS: Software managed (handle CS manually)
 * For details, refer to the TM7711 datasheet
 * and this site `https://ithelp.ithome.com.tw/articles/10278020`
 *
 * DMA Configuration:
 * - SPIx_RX: Peripheral to Memory, Data Width: Byte, Memory Increment: Enable
 * - SPIx_TX: Memory to Peripheral, Data Width: Byte, Memory Increment: Enable
 * - Priority: Medium or High recommended
 */
#define SPI_INSTANCE SPI1
extern SPI_HandleTypeDef hspi1;
#define SPI_HANDLE (&hspi1)
// You should use `SPI_COMP_FLAG` macro
extern volatile uint8_t SPI_Comp_Flag;
#define SPI_COMP_FLAG SPI_Comp_Flag
#define DOUT_STATUS HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) // MISO_Pin

#if (TM7711_DIFF_10_HZ + TM7711_TEMP_40_HZ + TM7711_DIFF_40_HZ) > 1
    #error "Only one TM7711 mode can be defined at a time"
#endif

#if (TM7711_DIFF_10_HZ + TM7711_TEMP_40_HZ + TM7711_DIFF_40_HZ) == 0
    #error "At least one TM7711 mode must be defined"
#endif

// 25 ticks
#if TM7711_DIFF_10_HZ
#define PRELOAD_INIT \
    { 0b00000000, 0b10101010, 0b10101010, 0b10101010, \
      0b10101010, 0b10101010, 0b10101010, 0b10000000 }
#elif TM7711_TEMP_40_HZ
// 26 ticks
#define PRELOAD_INIT \
    { 0b00000000, 0b10101010, 0b10101010, 0b10101010, \
      0b10101010, 0b10101010, 0b10101010, 0b10100000 }

#elif TM7711_DIFF_40_HZ
// 27 ticks
#define PRELOAD_INIT \
    { 0b00000000, 0b10101010, 0b10101010, 0b10101010, \
      0b10101010, 0b10101010, 0b10101010, 0b10101000 }
#endif

// Calibration factor
#define RAW_READING_1 1957000.0f
#define PRESSURE_READING_1 1.0f  // cmH20
#define RAW_READING_2 2614400.0f
#define PRESSURE_READING_2 13.5f  // cmH20
#define CALIBRATION_SLOPE \
    (PRESSURE_READING_2 - PRESSURE_READING_1) / \
    (RAW_READING_2 - RAW_READING_1)

float ConvertRawReadingToSignedFloat(uint32_t raw_reading);

uint32_t ConvertSPItoRawReading();

float ConvertRawToCmH2O(float raw_reading);

float ConvertCmH2OtoKPa(float cmh2o);

void TM7711_Read_Raw_DMA(void);

void TM7711_Read_Polling(void);

#endif //TM7711_H
