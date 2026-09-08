/*
 * dac7612.h
 *
 *  Created on: Sep 7, 2026
 *      Author: larsl
 */

#ifndef CORE_INC_DAC7612_H_
#define CORE_INC_DAC7612_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

typedef enum {
    DAC7612_CHANNEL_A = 0,
    DAC7612_CHANNEL_B = 1
} DAC7612_Channel;

typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *csPort;
    uint16_t      csPin;

    GPIO_TypeDef *ldacPort;
    uint16_t      ldacPin;

    float vref;  /* reference voltage feeding the DAC, for the voltage helpers */

    /* shadow of the last code written to each channel's input register,
     * so SetBoth / relative helpers don't need a readback (device has none) */
    uint16_t lastCode[2];
} DAC7612_HandleTypeDef;

#define DAC7612_MAX_CODE 0x0FFFu /* 12-bit full scale */

/**
 * @brief Initialize the handle and idle the CS/LDAC lines.
 * @param vref  Reference voltage used by the DAC (for SetVoltage helpers).
 *              Pass 0 if you only ever use raw codes.
 * CS and LDAC pins must already be configured as GPIO_OUTPUT_PP in CubeMX/your init code.
 */
HAL_StatusTypeDef DAC7612_Init(DAC7612_HandleTypeDef *hdac,
                                SPI_HandleTypeDef *hspi,
                                GPIO_TypeDef *csPort, uint16_t csPin,
                                GPIO_TypeDef *ldacPort, uint16_t ldacPin,
                                float vref);

/**
 * @brief Shift a 12-bit code into a channel's INPUT register only.
 *        Output does not change until DAC7612_LoadOutputs() is called.
 */
HAL_StatusTypeDef DAC7612_WriteChannelRaw(DAC7612_HandleTypeDef *hdac,
                                           DAC7612_Channel channel,
                                           uint16_t code12);

/**
 * @brief Pulse LDAC low, transferring both input registers to the
 *        outputs simultaneously.
 */
void DAC7612_LoadOutputs(DAC7612_HandleTypeDef *hdac);

/**
 * @brief Write one channel and update it immediately (write + pulse LDAC).
 */
HAL_StatusTypeDef DAC7612_SetChannel(DAC7612_HandleTypeDef *hdac,
                                      DAC7612_Channel channel,
                                      uint16_t code12);

/**
 * @brief Write both channels' input registers, then latch both at once
 *        with a single LDAC pulse. Use this for offset+gain when both
 *        need to move on the same edge.
 */
HAL_StatusTypeDef DAC7612_SetBoth(DAC7612_HandleTypeDef *hdac,
                                   uint16_t codeA, uint16_t codeB);

/**
 * @brief Convenience: convert a voltage into a 12-bit code (using the
 *        vref passed to Init) and write+update that channel immediately.
 *        Voltage is clamped to [0, vref].
 */
HAL_StatusTypeDef DAC7612_SetVoltage(DAC7612_HandleTypeDef *hdac,
                                      DAC7612_Channel channel,
                                      float voltage);

/**
 * @brief Read back the last code written to a channel (shadow value,
 *        not a real device readback - DAC7612 has no readback path).
 */
uint16_t DAC7612_GetLastCode(DAC7612_HandleTypeDef *hdac, DAC7612_Channel channel);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INC_DAC7612_H_ */
