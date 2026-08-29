/*
 * dre.h
 *
 *  Created on: Aug 30, 2026
 *      Author: larsl
 */

#ifndef CORE_INC_DRE_H_
#define CORE_INC_DRE_H_
#include "main.h"
typedef uint8_t bool;
typedef struct {
	uint32_t rawValue;
	uint32_t lastValue;
	int32_t value, startValue;
	bool button;
	bool buttonLastState;
	bool isToggle;
	void (*onClickUp)(uint8_t, void*, void*);
	void (*onClickDown)(uint8_t, void*, void*);
	void (*onButtonPress)(uint8_t, void*, void*);
	TIM_HandleTypeDef *htim;
	GPIO_TypeDef *buttonGPIOPort;
	uint16_t buttonGPIOPin;
	bool hasButton;
	bool valueDirty;
	uint16_t max;
	int16_t min;
	uint8_t countsPerDetent;
} dre_t;

void DRE_Init(dre_t* target, TIM_HandleTypeDef *htim, uint16_t max, uint16_t min,
		void (*onClickUp)(uint8_t, void*, void*), void (*onClickDown)(uint8_t, void*, void*),
		void (*onButtonPress)(uint8_t, void*, void*),
		GPIO_TypeDef *buttonGPIOPort, uint16_t buttonGPIOPin, bool toggle,
		int32_t startValue, bool hasButton, uint8_t countsPerDetent);

void DRE_Update(dre_t* dre, int argcRotation, void* argsRotation, int argcPress, void* argsPress,void* rotationResultOut, void* buttonPressResultOut);

#endif /* CORE_INC_DRE_H_ */
