/*
 * dre.c
 *
 *  Created on: Aug 30, 2026
 *      Author: larsl
 */
#include "dre.h"
static int32_t s_encoderAccum = 0;
static int32_t DRE_ConvertCountsToSteps(int32_t *accumulator,
		int32_t difference, int32_t countsPerDetent) {
	*accumulator += difference;

	int32_t steps = *accumulator / countsPerDetent;

	*accumulator %= countsPerDetent;

	return steps;
}
void DRE_Init(dre_t *target, TIM_HandleTypeDef *htim, uint16_t max,
		uint16_t min, void (*onClickUp)(uint8_t, void*, void*),
		void (*onClickDown)(uint8_t, void*, void*),
		void (*onButtonPress)(uint8_t, void*, void*),
		GPIO_TypeDef *buttonGPIOPort, uint16_t buttonGPIOPin, bool toggle,
		int32_t startValue, bool hasButton, uint8_t countsPerDetent) {
	dre_t new = { 0 };
	new.htim = htim;
	new.max = max;
	new.min = min;
	new.onClickUp = onClickUp;
	new.onClickDown = onClickDown;
	new.onButtonPress = onButtonPress;
	new.buttonGPIOPort = buttonGPIOPort;
	new.buttonGPIOPin = buttonGPIOPin;
	new.isToggle = toggle;
	new.startValue = startValue;
	__HAL_TIM_SET_COUNTER(htim, (uint16_t )startValue);
	new.value = startValue;
	new.hasButton = hasButton;
	new.countsPerDetent = countsPerDetent;
	memcpy(target, &new, sizeof(dre_t));
	HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
}
void DRE_Update(dre_t *dre, int argcRotation, void *argsRotation, int argcPress,
		void *argsPress, void *rotationResultOut, void *buttonPressResultOut) {
	dre->rawValue = __HAL_TIM_GET_COUNTER(dre->htim);
	if (dre->rawValue != dre->lastValue) {
		int32_t difference = (int16_t) (dre->rawValue - dre->lastValue);
		dre->value += DRE_ConvertCountsToSteps(&s_encoderAccum, difference,
				dre->countsPerDetent);
		if (dre->value > dre->max)
			dre->value = dre->max;
		if (dre->value < dre->min)
			dre->value = dre->min;
		if (difference < 0 && dre->onClickDown)
			dre->onClickDown(argcRotation, argsRotation, rotationResultOut);
		if (difference > 0 && dre->onClickUp)
			dre->onClickUp(argcRotation, argsRotation, rotationResultOut);
		dre->lastValue = dre->rawValue;
	}
	if (dre->hasButton) {
		bool pressed = HAL_GPIO_ReadPin(dre->buttonGPIOPort,
				dre->buttonGPIOPin);

		if (dre->isToggle) {
			if (pressed && !dre->buttonLastState) {
				dre->button = !dre->button;

				if (dre->onButtonPress)
					dre->onButtonPress(argcPress, argsPress,
							buttonPressResultOut);
			}
		} else {
			if (dre->button != pressed) {
				dre->button = pressed;

				if (dre->onButtonPress)
					dre->onButtonPress(argcPress, argsPress,
							buttonPressResultOut);
			}
		}

		dre->buttonLastState = pressed;
	}
}

