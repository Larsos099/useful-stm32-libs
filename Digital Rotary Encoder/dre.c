/*
 * dre.c
 *
 *  Created on: Aug 30, 2026
 *      Author: larsl
 */
#include "dre.h"
static int32_t DRE_ConvertCountsToSteps(int32_t *accumulator,
		int32_t difference, int32_t countsPerDetent) {
	if (countsPerDetent == 0)
		countsPerDetent = 1;
	*accumulator += difference;

	int32_t steps = *accumulator / countsPerDetent;

	*accumulator %= countsPerDetent;

	return steps;
}
void DRE_Init_ex(dre_t *target, TIM_HandleTypeDef *htim, int32_t max,
		int32_t min, void (*onClickUp)(uint8_t, void**, void*),
		void (*onClickDown)(uint8_t, void**, void*),
		void (*onButtonPress)(uint8_t, void**, void*),
		GPIO_TypeDef *buttonGPIOPort, uint16_t buttonGPIOPin, bool toggle,
		int32_t startValue, bool hasButton, uint8_t countsPerDetent,
		bool buttonActiveLow) {
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
	new.lastValue = startValue;
	new.hasButton = hasButton;
	new.countsPerDetent = countsPerDetent;
	new.lastValue = startValue;
	new.buttonActiveLow = buttonActiveLow;
	if (hasButton) {
		new.buttonLastState = HAL_GPIO_ReadPin(buttonGPIOPort, buttonGPIOPin);
	}
	memcpy(target, &new, sizeof(dre_t));
	HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
}
void DRE_Update_ex(dre_t *dre, int argcRotation, void **argsRotation,
		int argcPress, void **argsPress, void *rotationResultOut,
		void *buttonPressResultOut) {

	dre->rawValue = __HAL_TIM_GET_COUNTER(dre->htim);

	if (dre->rawValue != dre->lastValue) {
		dre->valueDirty = 1;
		int32_t difference = (int32_t) (dre->rawValue - dre->lastValue);

		int32_t steps = DRE_ConvertCountsToSteps(&dre->encoderAccum, difference,
				dre->countsPerDetent);

		dre->value += steps;

		if (dre->value > dre->max)
			dre->value = dre->max;

		if (dre->value < dre->min)
			dre->value = dre->min;

		if (steps < 0 && dre->onClickDown)
			dre->onClickDown(argcRotation, argsRotation, rotationResultOut);

		if (steps > 0 && dre->onClickUp)
			dre->onClickUp(argcRotation, argsRotation, rotationResultOut);

		dre->lastValue = dre->rawValue;
	}

	if (dre->hasButton) {

		GPIO_PinState pinState = HAL_GPIO_ReadPin(dre->buttonGPIOPort,
				dre->buttonGPIOPin);

		bool pressed;

		if (dre->buttonActiveLow)
			pressed = (pinState == GPIO_PIN_RESET);
		else
			pressed = (pinState == GPIO_PIN_SET);

		if (dre->isToggle) {

			if (pressed && !dre->buttonLastState) {
				dre->button = !dre->button;

				if (dre->onButtonPress)
					dre->onButtonPress(argcPress, argsPress,
							buttonPressResultOut);
			}
		} else {

			if (pressed && !dre->buttonLastState) {
				dre->button = true;

				if (dre->onButtonPress)
					dre->onButtonPress(argcPress, argsPress,
							buttonPressResultOut);
			}

			else if (!pressed && dre->buttonLastState) {
				dre->button = false;
			}
		}

		dre->buttonLastState = pressed;
	}
}

void DRE_Init(dre_t *target, TIM_HandleTypeDef *htim, int32_t max, int32_t min,
bool hasButton, bool isToggle, int32_t startValue, uint8_t countsPerDetent,
		bool buttonActiveLow) {
	dre_t new = { 0 };
	new.htim = htim;
	new.max = max;
	new.min = min;
	new.hasButton = hasButton;
	new.isToggle = isToggle;
	new.lastValue = new.value = startValue;
	new.countsPerDetent = countsPerDetent;
	new.buttonActiveLow = buttonActiveLow;
	memcpy(target, &new, sizeof(dre_t));
}

void DRE_Update(dre_t *dre) {
	dre->rawValue = __HAL_TIM_GET_COUNTER(dre->htim);

	if (dre->rawValue != dre->lastValue) {
		dre->valueDirty = 1;
		int32_t difference = (int32_t) (dre->rawValue - dre->lastValue);

		int32_t steps = DRE_ConvertCountsToSteps(&dre->encoderAccum, difference,
				dre->countsPerDetent);

		dre->value += steps;
		if (dre->value > dre->max)
			dre->value = dre->max;
		if (dre->value < dre->min)
			dre->value = dre->min;

		dre->lastValue = dre->rawValue;
	}

	if (dre->hasButton) {

		GPIO_PinState pinState = HAL_GPIO_ReadPin(dre->buttonGPIOPort,
				dre->buttonGPIOPin);

		bool pressed;

		if (dre->buttonActiveLow)
			pressed = (pinState == GPIO_PIN_RESET);
		else
			pressed = (pinState == GPIO_PIN_SET);

		if (dre->isToggle) {

			if (pressed && !dre->buttonLastState) {
				dre->button = !dre->button;
			}
		} else {

			if (pressed && !dre->buttonLastState) {
				dre->button = true;

			}

			else if (!pressed && dre->buttonLastState) {
				dre->button = false;
			}
		}

		dre->buttonLastState = pressed;
	}
}
