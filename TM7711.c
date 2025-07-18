//
// Created by brad on 7/17/25.
//

#include "TM7711.h"

#include "stm32l4xx_hal_spi.h"

#include <stdint.h>

#include "Tasks/uart_task.h"

volatile uint8_t SPI_Comp_Flag = 0; // Flag to indicate SPI transmission completion
uint8_t TxData[8] = PRELOAD_INIT;
uint8_t RxData[8] = {0};

/**
 * @brief Converts SPI received data to a raw 24-bit reading.
 * @return A 32-bit unsigned integer representing the raw reading.
 * @details
 * For detailed bit configuration, refer to the TM7711 datasheet.
 * [Datasheet](https://lcsc.com/datasheet/lcsc_datasheet_2006091008_TM-Shenzhen-Titan-Micro-Elec-TM7711_C601080.pdf)\n\n
 * The `PRELOAD_INIT` sequence prepares the TM7711 to send data. The device returns data
 * in 4-bit nibbles. The SPI interface is configured to capture data on the falling clock edge,
 * ensuring that each bit is stable and ready when read.
 * This function processes the six central bytes of the received data (`RxData[1]` to `RxData[6]`),
 * which together form the 24-bit ADC reading. The other bytes are dummy/control bytes and are
 * ignored, as specified in the datasheet.
 */
uint32_t ConvertSPItoRawReading() {
    // TODO disable interrupt before reading SPI data
    uint32_t result = 0;
    uint8_t nibble;

    for (int i = 0; i < 8; i++) {
        // because it sent bits from MSB to LSB
        nibble = 0;
        if (i >= 1 && i <= 6) {
            nibble |= (RxData[i] & 0b10000000) >> 4;
            nibble |= (RxData[i] & 0b00100000) >> 3;
            nibble |= (RxData[i] & 0b00001000) >> 2;
            nibble |= (RxData[i] & 0b00000010) >> 1;
            result |= (uint32_t)nibble << (6 - i) * 4; // shift to the correct position
        }
    }

    // TODO enable interrupt
    return result;
}

/**
 * @brief Converts a raw 24-bit reading to a signed float.
 * @param raw_reading The raw 24-bit reading as a 32-bit unsigned integer.
 * @return A signed float representation of the reading.
 * @details
 * The function checks if the sign bit (bit 23) is set. If it is, it extends the sign bit
 * to the full 32 bits by setting the upper byte to 0xFF. This allows for correct conversion
 * of negative values in two's complement format.
 */
float ConvertRawReadingToSignedFloat(uint32_t raw_reading) {
    // Convert the 24-bit reading to a signed float
    if (raw_reading & 0x800000) {
        // Check if the sign bit is set
        raw_reading |= 0xFF000000; // Extend the sign bit to 32 bits
    }
    return (float)raw_reading;
}

/**
 * @brief Maps a raw reading to cmH2O using linear interpolation.
 * @param raw_reading The raw pressure reading as a signed float
 * @return The pressure in cmH2O.
 * @details
 * This function uses two calibration points to map the raw reading to cmH2O:
 * It performs linear interpolation between these two points.
 */
float ConvertRawToCmH2O(float raw_reading) {
    // y = mx + b
    return CALIBRATION_SLOPE * (raw_reading - RAW_READING_1) + PRESSURE_READING_1;
}

/**
 * @brief Converts cmH2O to kPa.
 * @param cmh2o The pressure in cmH2O.
 * @return The pressure in kPa.
 * @details
 * This function converts cmH2O to kPa using the conversion factor:
 * 1 cmH2O = 0.0980665 kPa.
 */
float ConvertCmH2OtoKPa(float cmh2o) {
    return cmh2o * 0.0980665f; // 1 cmH2O = 0.0980665 kPa
}

// Weak function to handle SPI transmission completion
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI_INSTANCE) {
        // Set the completion flag, then you'll need to check it in your main loop
        SPI_COMP_FLAG = 1;
    }
}

void TM7711_Read_Raw_DMA(void) {
    HAL_SPI_TransmitReceive_DMA(SPI_HANDLE,
                                (uint8_t*)&TxData,
                                (uint8_t*)&RxData,
                                sizeof(uint8_t) * 8);
}

void TM7711_Read_Polling(void) {
    if (SPI_COMP_FLAG == 1 && DOUT_STATUS == GPIO_PIN_RESET) {
        // Clear the flag
        SPI_COMP_FLAG = 0;

        // Start the SPI transmission and reception
        TM7711_Read_Raw_DMA();
    }
}
