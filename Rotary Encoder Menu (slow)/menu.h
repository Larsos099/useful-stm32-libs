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

/* ---------------------------------------------------------------------
 * any_t: a small fixed-size value container. Same idea as before, minus
 * the type tag - a value's type is a property of the menu_row_t that
 * owns it, not of the storage itself, so it no longer lives here.
 * --------------------------------------------------------------------- */
typedef struct {
	alignas(max_align_t) unsigned char buf[ANY_MAX_SZ];
	size_t size;
	void (*dtor)(void*);
	void *ap; // Access Pointer, always == buf. Kept for API symmetry.
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
	MENU_FLOAT = 0, MENU_INT, MENU_SHORT, MENU_U64, MENU_STRING
} menu_value_type_e;

typedef enum {
	MENU_ROW_VALUE = 0, // any_t-backed scalar/string, printed as text
	MENU_ROW_ICON_LIST  // one of icons[], selected by iconIndex
} menu_row_kind_e;

typedef struct {
	const uint8_t *bitmap; // 1bpp, MSB-first, row-major, rows padded to a byte
	uint16_t width;
	uint16_t height;
	bool hasBgOverride;  // true = ignore the normal tab/row bg and use bgOverride instead
	uint16_t bgOverride; // backdrop color for this icon specifically, when hasBgOverride is set
	bool hasFgOverride;  // true = ignore the normal tab/row bg and use bgOverride instead
	uint16_t fgOverride; // backdrop color for this icon specifically, when hasBgOverride is set
} menu_icon_t;

typedef struct menu_row menu_row_t;

struct menu_row {
	const char *label;       // left-bound
	menu_row_kind_e kind;
	bool editable;            // false = display-only, skipped by the encoder(s)

	/* MENU_ROW_VALUE ----------------------------------------------- */
	any_t value;
	menu_value_type_e typeinfo;
	const char *suffix;       // e.g. "Hz", "V" - appended after the value; NULL for none
	union {
		struct {
			int32_t min, max, step;
		} i; // MENU_INT / MENU_SHORT / MENU_U64
		struct {
			float min, max, step;
		} f; // MENU_FLOAT
	} range;

	/* MENU_ROW_ICON_LIST --------------------------------------------- */
	const menu_icon_t *icons;
	size_t iconCount;
	size_t iconIndex;

	/* Style, right-bound value/icon, left-bound label share these ---- */
	uint16_t bgColor, fgColor;

	/* Optional callbacks, fired from menu_update() -------------------- */
	void (*onChange)(menu_row_t *row, void *ctx); // every accepted delta
	void (*onCommit)(menu_row_t *row, void *ctx); // edit mode left (press-to-edit only)
	void *ctx;

	/* --- internal render/edit state, do not set directly --- */
	bool _editing;
	bool _renderedOnce;
	char _lastText[24];
	size_t _lastIconIndex;
};

typedef struct {
	const char *label;   // optional, not drawn by the tab bar itself
	menu_icon_t tabIcon;  // drawn as the tab's square icon
	menu_row_t *rows;
	size_t rowCount;
	size_t selection;     // focused row within this screen, persists across tab switches
	size_t scrollOffset;  // first visible row, managed internally
} menu_screen_t;

typedef struct {
	FontDef font;

	uint16_t rowHeight;           // 0 = auto: divide the row area evenly, no scrolling
	uint16_t rowPadX;
	uint16_t rowSeparatorHeight;  // 0 disables the per-row separator line
	uint16_t rowSeparatorColor;

	uint16_t tabBarHeight;          // 0 disables the tab bar entirely
	uint16_t tabSize;                // tabs are square: tabSize x tabSize
	uint16_t tabGap;
	uint16_t tabBarSeparatorHeight; // 0 disables
	uint16_t tabBarSeparatorColor;

	uint16_t tabBgColor, tabFgColor;             // inactive tabs
	uint16_t tabActiveBgColor, tabActiveFgColor; // the open tab
	uint16_t tabFocusBorderColor;                 // border around the open tab while it has focus

	uint16_t editAccentColor; // value text color while a row is being edited

	uint16_t bg; // clears areas outside the tabs/rows
} menu_layout_t;

typedef enum {
	MENU_INPUT_PRESS_TO_EDIT = 0, // navEncoder: rotate to move, press to enter/exit edit, rotate to adjust while in edit
	MENU_INPUT_DUAL_ENCODER       // navEncoder moves selection, valueEncoder adjusts it live, no edit mode
} menu_input_mode_e;

typedef enum {
	MENU_FOCUS_TABS = 0,
	MENU_FOCUS_ROWS,
	MENU_FOCUS_EDIT // only reachable in MENU_INPUT_PRESS_TO_EDIT
} menu_focus_e;

typedef struct {
	menu_screen_t *screens;
	size_t screenCount;
	size_t currentScreen;
	size_t tabScrollOffset;

	menu_focus_e focus;

	menu_input_mode_e inputMode;
	dre_t *navEncoder;
	dre_t *valueEncoder; // read only when inputMode == MENU_INPUT_DUAL_ENCODER; may be NULL otherwise

	menu_layout_t layout;
} menu_t;

/* Row constructors -------------------------------------------------------
 * Each of these fully (re-)initializes *row, including zeroing internal
 * render state, so they're also safe to call again later to redefine a
 * row's identity/range (e.g. switching units).
 */
void menu_row_init_value(menu_row_t *row, const char *label,
		menu_value_type_e typeinfo, const void *initial, size_t size,
		int32_t min, int32_t max, int32_t step, const char *suffix,
		bool editable);
void menu_row_init_float(menu_row_t *row, const char *label, float initial,
		float min, float max, float step, const char *suffix, bool editable);
void menu_row_init_string(menu_row_t *row, const char *label,
		const char *initial);
void menu_row_init_icon_list(menu_row_t *row, const char *label,
		const menu_icon_t *icons, size_t iconCount, size_t initialIndex,
		bool editable);
void menu_row_set_colors(menu_row_t *row, uint16_t fgColor, uint16_t bgColor);
void menu_row_set_callbacks(menu_row_t *row,
		void (*onChange)(menu_row_t*, void*),
		void (*onCommit)(menu_row_t*, void*), void *ctx);

/* Screen ------------------------------------------------------------------
 * tabIcon may be NULL if the screen will only ever be the sole screen in
 * a menu (tab bar disabled) or you don't need it visually distinguished.
 */
void menu_screen_init(menu_screen_t *screen, menu_row_t *rows,
		size_t rowCount, const menu_icon_t *tabIcon);

/* Menu ---------------------------------------------------------------- */
menu_layout_t menu_layout_default(void);

void menu_init(menu_t *menu, menu_screen_t *screens, size_t screenCount,
		menu_input_mode_e inputMode, dre_t *navEncoder, dre_t *valueEncoder,
		menu_layout_t *layout /* NULL => menu_layout_default() */);

void menu_update(menu_t *menu); // call every loop iteration
void menu_render(menu_t *menu); // full redraw: tab bar + current screen

/* Programmatic value updates: set + redraw (if currently visible) in one call. */
void menu_set_row_value(menu_t *menu, size_t screenIndex, size_t rowIndex,
		const void *data, size_t size);
void menu_set_row_icon_index(menu_t *menu, size_t screenIndex,
		size_t rowIndex, size_t iconIndex);

#endif /* CORE_INC_MENU_H_ */
