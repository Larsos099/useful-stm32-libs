/*
 * dac7612.c
 *
 *  Created on: Sep 7, 2026
 *      Author: larsl
 */

#include "dac7612.h"

#define DAC7612_SPI_TIMEOUT_MS 10u

static inline void CS_Low(DAC7612_HandleTypeDef *hdac)
{
    HAL_GPIO_WritePin(hdac->csPort, hdac->csPin, GPIO_PIN_RESET);
}

static inline void CS_High(DAC7612_HandleTypeDef *hdac)
{
    HAL_GPIO_WritePin(hdac->csPort, hdac->csPin, GPIO_PIN_SET);
}

HAL_StatusTypeDef DAC7612_Init(DAC7612_HandleTypeDef *hdac,
                                SPI_HandleTypeDef *hspi,
                                GPIO_TypeDef *csPort, uint16_t csPin,
                                GPIO_TypeDef *ldacPort, uint16_t ldacPin,
                                float vref)
{
    if (hdac == NULL || hspi == NULL || csPort == NULL || ldacPort == NULL) {
        return HAL_ERROR;
    }

    hdac->hspi      = hspi;
    hdac->csPort    = csPort;
    hdac->csPin     = csPin;
    hdac->ldacPort  = ldacPort;
    hdac->ldacPin   = ldacPin;
    hdac->vref      = vref;
    hdac->lastCode[DAC7612_CHANNEL_A] = 0;
    hdac->lastCode[DAC7612_CHANNEL_B] = 0;

    /* Idle state: CS high (deselected), LDAC high (outputs not updating) */
    HAL_GPIO_WritePin(csPort, csPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ldacPort, ldacPin, GPIO_PIN_SET);

    return HAL_OK;
}

HAL_StatusTypeDef DAC7612_WriteChannelRaw(DAC7612_HandleTypeDef *hdac,
                                           DAC7612_Channel channel,
                                           uint16_t code12)
{
    if (hdac == NULL) {
        return HAL_ERROR;
    }

    code12 &= DAC7612_MAX_CODE;

    /* DB15 = X(0), DB14 = A0, DB13..DB2 = data, DB1..DB0 = X(0) */
    uint16_t word = ((uint16_t)(channel & 0x01u) << 14) | (code12 << 2);
    uint8_t txBuf[2] = { (uint8_t)(word >> 8), (uint8_t)(word & 0xFF) };

    CS_Low(hdac);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hdac->hspi, txBuf, 2, DAC7612_SPI_TIMEOUT_MS);
    CS_High(hdac);

    if (status == HAL_OK) {
        hdac->lastCode[channel] = code12;
    }

    return status;
}

void DAC7612_LoadOutputs(DAC7612_HandleTypeDef *hdac)
{
    /* Active-low pulse. Min LDAC pulse width is a few ns per the datasheet,
     * so back-to-back HAL_GPIO_WritePin calls are already wide enough at
     * typical MCU clock speeds - no delay needed. */
    HAL_GPIO_WritePin(hdac->ldacPort, hdac->ldacPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(hdac->ldacPort, hdac->ldacPin, GPIO_PIN_SET);
}

HAL_StatusTypeDef DAC7612_SetChannel(DAC7612_HandleTypeDef *hdac,
                                      DAC7612_Channel channel,
                                      uint16_t code12)
{
    HAL_StatusTypeDef status = DAC7612_WriteChannelRaw(hdac, channel, code12);
    if (status == HAL_OK) {
        DAC7612_LoadOutputs(hdac);
    }
    return status;
}

HAL_StatusTypeDef DAC7612_SetBoth(DAC7612_HandleTypeDef *hdac,
                                   uint16_t codeA, uint16_t codeB)
{
    HAL_StatusTypeDef status = DAC7612_WriteChannelRaw(hdac, DAC7612_CHANNEL_A, codeA);
    if (status != HAL_OK) {
        return status;
    }

    status = DAC7612_WriteChannelRaw(hdac, DAC7612_CHANNEL_B, codeB);
    if (status != HAL_OK) {
        return status;
    }

    DAC7612_LoadOutputs(hdac);
    return HAL_OK;
}

HAL_StatusTypeDef DAC7612_SetVoltage(DAC7612_HandleTypeDef *hdac,
                                      DAC7612_Channel channel,
                                      float voltage)
{
    if (hdac->vref <= 0.0f) {
        return HAL_ERROR; /* Init() was never given a vref */
    }

    if (voltage < 0.0f) {
        voltage = 0.0f;
    } else if (voltage > hdac->vref) {
        voltage = hdac->vref;
    }

    uint16_t code = (uint16_t)((voltage / hdac->vref) * (float)DAC7612_MAX_CODE + 0.5f);
    return DAC7612_SetChannel(hdac, channel, code);
}

uint16_t DAC7612_GetLastCode(DAC7612_HandleTypeDef *hdac, DAC7612_Channel channel)
{
    return hdac->lastCode[channel];
}
