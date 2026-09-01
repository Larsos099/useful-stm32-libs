/*
 * dre.h
 *
 *  Created on: Aug 30, 2026
 *      Author: larsl
 */

#ifndef CORE_INC_DRE_H_
#define CORE_INC_DRE_H_
#include "main.h"
#include <stdbool.h>
typedef struct {
	uint32_t rawValue;
	uint32_t lastRawValue, lastValue;
	int32_t value, startValue;
	bool button;
	bool buttonLastState;
	bool isToggle;
	int32_t encoderAccum;
	void (*onClickUp)(uint8_t, void**, void*);
	void (*onClickDown)(uint8_t, void**, void*);
	void (*onButtonPress)(uint8_t, void**, void*);
	TIM_HandleTypeDef *htim;
	GPIO_TypeDef *buttonGPIOPort;
	uint16_t buttonGPIOPin;
	bool hasButton;
	bool valueDirty;
	int32_t min;
	int32_t max;
	uint8_t countsPerDetent;
	bool buttonActiveLow;
} dre_t;

void DRE_Init_ex(dre_t *target, TIM_HandleTypeDef *htim, int32_t max,
		int32_t min, void (*onClickUp)(uint8_t, void**, void*),
		void (*onClickDown)(uint8_t, void**, void*),
		void (*onButtonPress)(uint8_t, void**, void*),
		GPIO_TypeDef *buttonGPIOPort, uint16_t buttonGPIOPin, bool toggle,
		int32_t startValue, bool hasButton, uint8_t countsPerDetent,
		bool buttonActiveLow);

void DRE_Init(dre_t *target, TIM_HandleTypeDef *htim, int32_t max, int32_t min,
		bool hasButton, bool isToggle, int32_t startValue,
		uint8_t countsPerDetent, bool buttonActiveLow);

void DRE_Update_ex(dre_t *dre, int argcRotation, void **argsRotation,
		int argcPress, void **argsPress, void *rotationResultOut,
		void *buttonPressResultOut);

void DRE_Update(dre_t* dre);
/*
 * Lightweight polling helpers for an encoder that is dedicated to a single
 * consumer (e.g. a menu_t). Unlike DRE_Update()/DRE_Update_ex(), these do
 * NOT touch value/min/max/button/isToggle - they only report what changed
 * since the last call, using the same rawValue/lastRawValue/encoderAccum
 * and buttonLastState bookkeeping fields.
 *
 * Do not mix these with DRE_Update()/DRE_Update_ex() on the same dre_t -
 * both styles update the same bookkeeping fields and would fight over them.
 * Dedicate one dre_t per consumer.
 */

int32_t DRE_ReadStepDelta(dre_t *dre);
bool DRE_ReadButtonEdge(dre_t *dre);
#endif /* CORE_INC_DRE_H_ */
