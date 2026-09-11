/*
 * menu.h
 *
 *  Created on: Sep 1, 2026
 *      Author: larsl
 */

#ifndef CORE_INC_MENU_H_
#define CORE_INC_MENU_H_

#include <stdbool.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include <stddef.h>

#include "fonts.h"
#include "dre.h"

#define ANY_MAX_SZ 64

typedef struct {
	alignas(max_align_t) unsigned char buf[ANY_MAX_SZ];
	size_t size;
	void (*dtor)(void*);
	void *ap;
} any_t;

static inline void any_make(const void *data, size_t size,
		void (*dtor)(void*), any_t *out) {
	if (!out || size > ANY_MAX_SZ)
		return;

	memset(out, 0, sizeof(*out));

	if (data && size)
		memcpy(out->buf, data, size);

	out->size = size;
	out->dtor = dtor;
	out->ap = out->buf;
}

static inline void any_destroy(any_t *any) {
	if (any->dtor)
		any->dtor(any->buf);
	any->dtor = NULL;
}

typedef enum {
	MENU_FLOAT = 0, MENU_INT, MENU_SHORT, MENU_U64, MENU_STRING, MENU_BOOL,
	MENU_SDFLOAT, MENU_ENUM_STR
} menu_value_type_e;

typedef enum {
	MENU_ROW_VALUE = 0,
	MENU_ROW_ICON_LIST
} menu_row_kind_e;

typedef struct {
	const uint8_t *bitmap;
	uint16_t width;
	uint16_t height;
	bool hasBgOverride;
	uint16_t bgOverride;
	bool hasFgOverride;
	uint16_t fgOverride;
} menu_icon_t;

typedef struct menu_row menu_row_t;

struct menu_row {
	const char *label;
	menu_row_kind_e kind;
	bool editable;

	/* MENU_ROW_VALUE ----------------------------------------------- */
	any_t value;
	menu_value_type_e typeinfo;
	const char *suffix;
	union {
		struct {
			int32_t min, max, step;
		} i;
		struct {
			float min, max, step;
		} f;
		struct {
			const int32_t *values;
			const char * const *strings;
			size_t count;
		} e;
	} range;
	uint8_t sdIntDigits;
	uint8_t sdDecDigits;

	/* MENU_ROW_ICON_LIST --------------------------------------------- */
	const menu_icon_t *icons;
	size_t iconCount;
	size_t iconIndex;

	/* Style ---------------------------------------------------------- */
	uint16_t bgColor, fgColor;

	/* Callbacks ------------------------------------------------------ */
	void (*onChange)(menu_row_t *row, void *ctx);
	void (*onCommit)(menu_row_t *row, void *ctx);
	void *ctx;

	/* Internal State ------------------------------------------------- */
	bool _editing;
	bool _renderedOnce;
	char _lastText[24];
	size_t _lastIconIndex;
	size_t _sdDigit;
	size_t _enumIndex;
	bool _toggled;
};

typedef struct {
	const char *label;
	menu_icon_t tabIcon;
	menu_row_t *rows;
	size_t rowCount;
	size_t selection;
	size_t scrollOffset;

	/* Top-Right Float Display */
	bool hasTopRightFloat;
	const char *topRightLabel;
	const float *topRightValPtr;
	float topRightValue;
	uint8_t topRightDecimals;
	const char *topRightSuffix;
	float _lastTopRightValue;
	bool _topRightRendered;
} menu_screen_t;

typedef struct {
	FontDef font;

	uint16_t rowHeight;
	uint16_t rowPadX;
	uint16_t rowSeparatorHeight;
	uint16_t rowSeparatorColor;

	uint16_t tabBarHeight;
	uint16_t tabSize;
	uint16_t tabGap;
	uint16_t tabBarSeparatorHeight;
	uint16_t tabBarSeparatorColor;

	uint16_t tabBgColor, tabFgColor;
	uint16_t tabActiveBgColor, tabActiveFgColor;
	uint16_t tabFocusBorderColor;

	uint16_t editAccentColor;
	bool showToggleAccent;

	uint16_t bg;
} menu_layout_t;

typedef enum {
	MENU_INPUT_PRESS_TO_EDIT = 0,
	MENU_INPUT_DUAL_ENCODER
} menu_input_mode_e;

typedef enum {
	MENU_FOCUS_TABS = 0,
	MENU_FOCUS_ROWS,
	MENU_FOCUS_EDIT
} menu_focus_e;

typedef struct {
	menu_screen_t *screens;
	size_t screenCount;
	size_t currentScreen;
	size_t tabScrollOffset;
	uint8_t cs;
	menu_focus_e focus;

	menu_input_mode_e inputMode;
	dre_t *navEncoder;
	dre_t *valueEncoder;

	menu_layout_t layout;

	/* Internal state: dual-encoder-mode nav-button press/hold tracking,
	 * used to distinguish a short click from a >=2s hold that opens the
	 * tab bar (see MENU_TAB_HOLD_MS in menu.c). Not part of the public API. */
	bool _navBtnHeld;
	bool _navLongPressFired;
	uint32_t _navBtnDownTick;
} menu_t;

void menu_row_init_value(menu_row_t *row, const char *label,
		menu_value_type_e typeinfo, const void *initial, size_t size,
		int32_t min, int32_t max, int32_t step, const char *suffix,
		bool editable);
void menu_row_init_float(menu_row_t *row, const char *label, float initial,
		float min, float max, float step, const char *suffix, bool editable);
void menu_row_init_sdfloat(menu_row_t *row, const char *label, float initial,
		float min, float max, uint8_t intDigits, uint8_t decDigits, const char *suffix,
		bool editable);
void menu_row_init_string(menu_row_t *row, const char *label,
		const char *initial);
void menu_row_init_icon_list(menu_row_t *row, const char *label,
		const menu_icon_t *icons, size_t iconCount, size_t initialIndex,
		bool editable);
void menu_row_init_boolean(menu_row_t *row, const char *label,
		bool initial, bool editable);
void menu_row_init_enum_str(menu_row_t *row, const char *label,
		const int32_t *values, const char * const *strings, size_t count,
		size_t initialIndex, bool editable);
void menu_row_set_colors(menu_row_t *row, uint16_t fgColor, uint16_t bgColor);
void menu_row_set_callbacks(menu_row_t *row,
		void (*onChange)(menu_row_t*, void*),
		void (*onCommit)(menu_row_t*, void*), void *ctx);

void menu_screen_init(menu_screen_t *screen, menu_row_t *rows,
		size_t rowCount, const menu_icon_t *tabIcon);
void menu_screen_enable_top_right_float(menu_screen_t *screen, const char *label,
		const float *valPtr, uint8_t decimals, const char *suffix);
void menu_set_top_right_float_value(menu_t *menu, size_t screenIndex, float value);
void menu_set_top_right_float_suffix(menu_t *menu, size_t screenIndex, const char *suffix);
void menu_render_top_right_float(menu_t *menu);

menu_layout_t menu_layout_default(void);

void menu_init(menu_t *menu, menu_screen_t *screens, size_t screenCount,
		menu_input_mode_e inputMode, dre_t *navEncoder, dre_t *valueEncoder,
		menu_layout_t *layout);

void menu_update(menu_t *menu);
void menu_render(menu_t *menu);

void menu_set_row_value(menu_t *menu, size_t screenIndex, size_t rowIndex,
		const void *data, size_t size);
void menu_set_row_icon_index(menu_t *menu, size_t screenIndex,
		size_t rowIndex, size_t iconIndex);

/* New: Change editability on the fly */
void menu_set_row_editable(menu_t *menu, size_t screenIndex, size_t rowIndex, bool editable);

#endif /* CORE_INC_MENU_H_ */
