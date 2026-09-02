/*
 * menu.c
 *
 *  Created on: Sep 1, 2026
 *      Author: larsl
 */

#include "menu.h"
#include "st7789.h"
#include <stdio.h>

/* Defined in st7789.c but not exposed via st7789.h; reused here so float
 * values don't need printf's %f (avoids the newlib-nano float-printf issue). */
extern void LCD_FloatToString(float num, char *buf, uint8_t decimals);

/* Layout / style tuning for values not exposed via menu_layout_t ---------*/
#define MENU_FLOAT_DECIMALS 2
#define MENU_VALUE_STR_SZ   24

/* ------------------------------------------------------------------------
 * Forward declarations (definition order below follows the public API,
 * geometry helpers are used before they're textually defined otherwise).
 * ------------------------------------------------------------------------ */
static uint16_t menu_row_area_top(const menu_t *m);
static uint16_t menu_row_area_height(const menu_t *m);
static uint16_t menu_effective_row_height(const menu_t *m,
		const menu_screen_t *s);
static size_t menu_visible_rows(const menu_t *m, const menu_screen_t *s);
static void menu_clamp_row_scroll(const menu_t *m, menu_screen_t *s);
static size_t menu_visible_tabs(const menu_t *m);
static void menu_clamp_tab_scroll(menu_t *m);
static void menu_row_rect(const menu_t *m, const menu_screen_t *s,
		size_t visIdx, uint16_t *top, uint16_t *h);
static void menu_value_to_string(const menu_row_t *row, char *out,
		size_t out_sz);
static void menu_render_row_full(menu_t *m, menu_screen_t *s, size_t rowIdx,
		size_t visIdx);
static void menu_render_row_value_only(menu_t *m, menu_screen_t *s,
		size_t rowIdx, size_t visIdx);
static void menu_render_tabs(menu_t *m);
static void menu_render_screen(menu_t *m);
static void menu_switch_screen(menu_t *m, size_t newIndex);
static void menu_move_tab(menu_t *m, int32_t delta);
static void menu_move_row(menu_t *m, menu_screen_t *s, int32_t delta);
static void menu_apply_delta(menu_row_t *row, int32_t delta);

/* =========================================================================
 * Geometry
 * ========================================================================= */

static uint16_t menu_row_area_top(const menu_t *m) {
	uint16_t top = 0;
	if (m->layout.tabBarHeight) {
		top += m->layout.tabBarHeight;
		top += m->layout.tabBarSeparatorHeight;
	}
	return top;
}

static uint16_t menu_row_area_height(const menu_t *m) {
	uint16_t top = menu_row_area_top(m);
	return (top < ST7789_HEIGHT) ? (uint16_t) (ST7789_HEIGHT - top) : 0;
}

static uint16_t menu_effective_row_height(const menu_t *m,
		const menu_screen_t *s) {
	uint16_t areaH = menu_row_area_height(m);
	if (m->layout.rowHeight)
		return m->layout.rowHeight;
	if (s->rowCount == 0)
		return areaH;
	uint16_t h = (uint16_t) (areaH / s->rowCount);
	return h ? h : 1;
}

static size_t menu_visible_rows(const menu_t *m, const menu_screen_t *s) {
	uint16_t rh = menu_effective_row_height(m, s);
	if (rh == 0)
		return 0;
	size_t n = menu_row_area_height(m) / rh;
	if (n > s->rowCount)
		n = s->rowCount;
	return n ? n : (s->rowCount ? 1 : 0);
}

static void menu_clamp_row_scroll(const menu_t *m, menu_screen_t *s) {
	size_t visible = menu_visible_rows(m, s);
	if (s->rowCount == 0 || s->rowCount <= visible) {
		s->scrollOffset = 0;
		return;
	}
	size_t maxOffset = s->rowCount - visible;
	if (s->selection < s->scrollOffset)
		s->scrollOffset = s->selection;
	else if (s->selection >= s->scrollOffset + visible)
		s->scrollOffset = s->selection - visible + 1;
	if (s->scrollOffset > maxOffset)
		s->scrollOffset = maxOffset;
}

static size_t menu_visible_tabs(const menu_t *m) {
	uint16_t pitch = (uint16_t) (m->layout.tabSize + m->layout.tabGap);
	if (pitch == 0)
		return m->screenCount;
	size_t n = ST7789_WIDTH / pitch;
	if (n > m->screenCount)
		n = m->screenCount;
	return n ? n : (m->screenCount ? 1 : 0);
}

static void menu_clamp_tab_scroll(menu_t *m) {
	size_t visible = menu_visible_tabs(m);
	if (m->screenCount == 0 || m->screenCount <= visible) {
		m->tabScrollOffset = 0;
		return;
	}
	size_t maxOffset = m->screenCount - visible;
	if (m->currentScreen < m->tabScrollOffset)
		m->tabScrollOffset = m->currentScreen;
	else if (m->currentScreen >= m->tabScrollOffset + visible)
		m->tabScrollOffset = m->currentScreen - visible + 1;
	if (m->tabScrollOffset > maxOffset)
		m->tabScrollOffset = maxOffset;
}

static void menu_row_rect(const menu_t *m, const menu_screen_t *s,
		size_t visIdx, uint16_t *top, uint16_t *h) {
	uint16_t rh = menu_effective_row_height(m, s);
	uint16_t areaTop = menu_row_area_top(m);
	*top = (uint16_t) (areaTop + visIdx * rh);
	*h = rh;

	if (m->layout.rowHeight == 0) {
		/* auto mode: all rows are visible (no scrolling), so give the
		 * last one whatever remains to reach the bottom of the panel
		 * regardless of integer-division rounding. */
		if (s->rowCount && visIdx == s->rowCount - 1) {
			uint16_t bottom = ST7789_HEIGHT;
			*h = (bottom > *top) ? (uint16_t) (bottom - *top) : rh;
		}
	}
}

/* =========================================================================
 * Value formatting
 * ========================================================================= */

static void menu_value_to_string(const menu_row_t *row, char *out,
		size_t out_sz) {
	if (!out || out_sz == 0)
		return;
	out[0] = '\0';
	if (!row)
		return;

	switch (row->typeinfo) {
	case MENU_FLOAT: {
		float f;
		memcpy(&f, row->value.ap, sizeof f);
		LCD_FloatToString(f, out, MENU_FLOAT_DECIMALS);
		break;
	}
	case MENU_INT: {
		int v;
		memcpy(&v, row->value.ap, sizeof v);
		snprintf(out, out_sz, "%d", v);
		break;
	}
	case MENU_SHORT: {
		short v;
		memcpy(&v, row->value.ap, sizeof v);
		snprintf(out, out_sz, "%d", (int) v);
		break;
	}
	case MENU_U64: {
		uint64_t v;
		memcpy(&v, row->value.ap, sizeof v);
		snprintf(out, out_sz, "%llu", (unsigned long long) v);
		break;
	}
	case MENU_STRING: {
		size_t n =
				(row->value.size < out_sz - 1) ? row->value.size : (out_sz - 1);
		memcpy(out, row->value.ap, n);
		out[n] = '\0';
		break;
	}
	}

	if (row->suffix && row->suffix[0]) {
		size_t len = strlen(out);
		size_t slen = strlen(row->suffix);
		if (len + slen < out_sz)
			memcpy(out + len, row->suffix, slen + 1);
	}
}

/* =========================================================================
 * Rendering
 * ========================================================================= */

static void menu_render_row_full(menu_t *m, menu_screen_t *s, size_t rowIdx,
		size_t visIdx) {
	menu_row_t *row = &s->rows[rowIdx];
	uint16_t top, h;
	menu_row_rect(m, s, visIdx, &top, &h);

	bool selected = (rowIdx == s->selection) && (m->focus != MENU_FOCUS_TABS);
	uint16_t bg = selected ? row->fgColor : row->bgColor;
	uint16_t fg = selected ? row->bgColor : row->fgColor;

	uint16_t sep = m->layout.rowSeparatorHeight;
	uint16_t contentH = (h > sep) ? (uint16_t) (h - sep) : h;

	if (contentH && top < ST7789_HEIGHT) {
		uint16_t bottom = (uint16_t) (top + contentH - 1);
		if (bottom >= ST7789_HEIGHT)
			bottom = ST7789_HEIGHT - 1;
		ST7789_Fill(0, top, ST7789_WIDTH - 1, bottom, bg);
	}
	if (contentH < h) {
		uint16_t sepBottom = (uint16_t) (top + h - 1);
		if (sepBottom >= ST7789_HEIGHT)
			sepBottom = ST7789_HEIGHT - 1;
		ST7789_Fill(0, top + contentH, ST7789_WIDTH - 1, sepBottom,
				m->layout.rowSeparatorColor);
	}

	const FontDef font = m->layout.font;
	uint16_t textY = (uint16_t) (top
			+ ((contentH > font.height) ? (contentH - font.height) / 2 : 0));

	ST7789_WriteString_Fast(m->layout.rowPadX, textY,
			row->label ? row->label : "", font, fg, bg);

	if (row->kind == MENU_ROW_ICON_LIST && row->iconCount) {
		const menu_icon_t *icon = &row->icons[row->iconIndex];
		uint16_t iw = icon->width, ih = icon->height;
		uint16_t ix = (iw + m->layout.rowPadX < ST7789_WIDTH) ?
		              (ST7789_WIDTH - m->layout.rowPadX - iw) :
		              m->layout.rowPadX;
		uint16_t iy = (uint16_t) (top
				+ ((contentH > ih) ? (contentH - ih) / 2 : 0));
		if (icon->bitmap)
			ST7789_DrawBitmap1BPP(ix, iy, iw, ih, icon->bitmap, fg, bg);
		row->_lastIconIndex = row->iconIndex;
	} else {
		char text[MENU_VALUE_STR_SZ];
		menu_value_to_string(row, text, sizeof text);
		uint16_t textFg = row->_editing ? m->layout.editAccentColor : fg;
		uint16_t w = (uint16_t) (strlen(text) * font.width);
		uint16_t vx =
				(uint16_t) (w + m->layout.rowPadX < ST7789_WIDTH) ?
						(uint16_t) (ST7789_WIDTH - m->layout.rowPadX - w) :
						m->layout.rowPadX;
		ST7789_WriteString_Fast(vx, textY, text, font, textFg, bg);
		strncpy(row->_lastText, text, sizeof(row->_lastText) - 1);
		row->_lastText[sizeof(row->_lastText) - 1] = '\0';
	}

	row->_renderedOnce = true;
}

static void menu_render_row_value_only(menu_t *m, menu_screen_t *s,
		size_t rowIdx, size_t visIdx) {
	menu_row_t *row = &s->rows[rowIdx];
	uint16_t top, h;
	menu_row_rect(m, s, visIdx, &top, &h);
	uint16_t sep = m->layout.rowSeparatorHeight;
	uint16_t contentH = (h > sep) ? (uint16_t) (h - sep) : h;

	bool selected = (rowIdx == s->selection) && (m->focus != MENU_FOCUS_TABS);
	uint16_t bg = selected ? row->fgColor : row->bgColor;
	uint16_t fg = selected ? row->bgColor : row->fgColor;

	const FontDef font = m->layout.font;
	uint16_t textY = (uint16_t) (top
			+ ((contentH > font.height) ? (contentH - font.height) / 2 : 0));

	if (row->kind == MENU_ROW_ICON_LIST && row->iconCount) {
		const menu_icon_t *icon = &row->icons[row->iconIndex];
		uint16_t iw = icon->width, ih = icon->height;
		uint16_t ix =
				(uint16_t) (iw + m->layout.rowPadX < ST7789_WIDTH) ?
						(uint16_t) (ST7789_WIDTH - m->layout.rowPadX - iw) :
						m->layout.rowPadX;
		uint16_t iy = (uint16_t) (top
				+ ((contentH > ih) ? (contentH - ih) / 2 : 0));

		ST7789_Fill(ix, iy, ST7789_WIDTH - m->layout.rowPadX - 1, iy + ih - 1,
				bg);
		if (icon->bitmap)
			ST7789_DrawBitmap1BPP(ix, iy, iw, ih, icon->bitmap, fg, bg);
		row->_lastIconIndex = row->iconIndex;
		return;
	}

	char text[MENU_VALUE_STR_SZ];
	menu_value_to_string(row, text, sizeof text);

	size_t oldLen = row->_renderedOnce ? strlen(row->_lastText) : 0;
	size_t newLen = strlen(text);
	size_t maxLen = (oldLen > newLen) ? oldLen : newLen;
	uint16_t clearW = (uint16_t) (maxLen * font.width);
	uint16_t clearX =
			(uint16_t) (clearW + m->layout.rowPadX < ST7789_WIDTH) ?
					(uint16_t) (ST7789_WIDTH - m->layout.rowPadX - clearW) :
					m->layout.rowPadX;

	ST7789_Fill(clearX, textY, ST7789_WIDTH - m->layout.rowPadX - 1,
			textY + font.height - 1, bg);

	uint16_t textFg = row->_editing ? m->layout.editAccentColor : fg;
	uint16_t w = (uint16_t) (newLen * font.width);
	uint16_t vx =
			(uint16_t) (w + m->layout.rowPadX < ST7789_WIDTH) ?
					(uint16_t) (ST7789_WIDTH - m->layout.rowPadX - w) :
					m->layout.rowPadX;
	ST7789_WriteString_Fast(vx, textY, text, font, textFg, bg);

	strncpy(row->_lastText, text, sizeof(row->_lastText) - 1);
	row->_lastText[sizeof(row->_lastText) - 1] = '\0';
	row->_renderedOnce = true;
}

static void menu_render_tabs(menu_t *m) {
	if (!m->layout.tabBarHeight || m->screenCount == 0)
		return;

	menu_clamp_tab_scroll(m);

	ST7789_Fill(0, 0, ST7789_WIDTH - 1, m->layout.tabBarHeight - 1,
			m->layout.bg);

	size_t visible = menu_visible_tabs(m);
	uint16_t pitch = (uint16_t) (m->layout.tabSize + m->layout.tabGap);
	uint16_t y =
			(m->layout.tabBarHeight > m->layout.tabSize) ?
					(uint16_t) ((m->layout.tabBarHeight - m->layout.tabSize) / 2) :
					0;

	for (size_t i = 0; i < visible && (m->tabScrollOffset + i) < m->screenCount;
			i++) {
		size_t idx = m->tabScrollOffset + i;
		menu_screen_t *scr = &m->screens[idx];
		uint16_t x = (uint16_t) (m->layout.tabGap + i * pitch);
		bool active = (idx == m->currentScreen);
		uint16_t bg =
				active ? m->layout.tabActiveBgColor : m->layout.tabBgColor;
		uint16_t fg =
				active ? m->layout.tabActiveFgColor : m->layout.tabFgColor;
		uint16_t tabBg = scr->tabIcon.hasBgOverride ?
						scr->tabIcon.bgOverride : bg;
		uint16_t tabFg = scr->tabIcon.hasFgOverride ?
								scr->tabIcon.fgOverride : fg;


		ST7789_Fill(x, y, x + m->layout.tabSize - 1, y + m->layout.tabSize - 1,
				bg);

		if (scr->tabIcon.bitmap && scr->tabIcon.width && scr->tabIcon.height) {
			uint16_t iw = scr->tabIcon.width, ih = scr->tabIcon.height;
			uint16_t ix =
					(uint16_t) (x
							+ (iw < m->layout.tabSize ?
									(m->layout.tabSize - iw) / 2 : 0));
			uint16_t iy =
					(uint16_t) (y
							+ (ih < m->layout.tabSize ?
									(m->layout.tabSize - ih) / 2 : 0));
			ST7789_DrawBitmap1BPP(ix, iy, iw, ih, scr->tabIcon.bitmap, tabFg, tabBg);
		}

		if (active && m->focus == MENU_FOCUS_TABS) {
			uint16_t bx0 = (x > 0) ? (uint16_t) (x - 1) : 0;
			uint16_t by0 = (y > 0) ? (uint16_t) (y - 1) : 0;
			uint16_t bx1 = (uint16_t) (x + m->layout.tabSize);
			uint16_t by1 = (uint16_t) (y + m->layout.tabSize);
			if (bx1 >= ST7789_WIDTH)
				bx1 = ST7789_WIDTH - 1;
			if (by1 >= ST7789_HEIGHT)
				by1 = ST7789_HEIGHT - 1;
			uint16_t c = m->layout.tabFocusBorderColor;
			ST7789_Fill(bx0, by0, bx1, by0, c); // top
			ST7789_Fill(bx0, by1, bx1, by1, c); // bottom
			ST7789_Fill(bx0, by0, bx0, by1, c); // left
			ST7789_Fill(bx1, by0, bx1, by1, c); // right
		}
	}

	if (m->layout.tabBarSeparatorHeight) {
		uint16_t sepBottom = (uint16_t) (m->layout.tabBarHeight
				+ m->layout.tabBarSeparatorHeight - 1);
		if (sepBottom >= ST7789_HEIGHT)
			sepBottom = ST7789_HEIGHT - 1;
		ST7789_Fill(0, m->layout.tabBarHeight, ST7789_WIDTH - 1, sepBottom,
				m->layout.tabBarSeparatorColor);
	}
}

static void menu_render_screen(menu_t *m) {
	if (m->screenCount == 0)
		return;
	menu_screen_t *s = &m->screens[m->currentScreen];
	menu_clamp_row_scroll(m, s);

	uint16_t areaTop = menu_row_area_top(m);
	uint16_t areaH = menu_row_area_height(m);
	if (areaH)
		ST7789_Fill(0, areaTop, ST7789_WIDTH - 1, areaTop + areaH - 1,
				m->layout.bg);

	size_t visible = menu_visible_rows(m, s);
	for (size_t i = 0; i < visible && (s->scrollOffset + i) < s->rowCount;
			i++) {
		menu_render_row_full(m, s, s->scrollOffset + i, i);
	}
}

void menu_render(menu_t *m) {
	if (!m || m->screenCount == 0)
		return;
	menu_render_tabs(m);
	menu_render_screen(m);
}

/* =========================================================================
 * Navigation
 * ========================================================================= */

static void menu_switch_screen(menu_t *m, size_t newIndex) {
	m->currentScreen = newIndex;
	menu_clamp_tab_scroll(m);
	menu_render_tabs(m);
	menu_render_screen(m);
}

static void menu_move_tab(menu_t *m, int32_t delta) {
	if (m->screenCount == 0)
		return;
	int32_t idx = (int32_t) m->currentScreen + delta;
	int32_t n = (int32_t) m->screenCount;
	idx %= n;
	if (idx < 0)
		idx += n;
	if ((size_t) idx != m->currentScreen)
		menu_switch_screen(m, (size_t) idx);
}

static void menu_move_row(menu_t *m, menu_screen_t *s, int32_t delta) {
	if (s->rowCount == 0)
		return;
	int32_t idx = (int32_t) s->selection + delta;
	int32_t n = (int32_t) s->rowCount;
	idx %= n;
	if (idx < 0)
		idx += n;
	if ((size_t) idx == s->selection)
		return;

	size_t oldSelection = s->selection;
	size_t oldScrollOffset = s->scrollOffset;

	s->selection = (size_t) idx;
	menu_clamp_row_scroll(m, s);

	size_t visible = menu_visible_rows(m, s);

	/* If scrolling occurred or layout shifted, a full screen render is required */
	if (s->scrollOffset != oldScrollOffset) {
		menu_render_screen(m);
	} else {
		/* Otherwise, only re-render the two affected rows (old and new selection) */
		if (oldSelection >= s->scrollOffset && oldSelection < s->scrollOffset + visible) {
			menu_render_row_full(m, s, oldSelection, oldSelection - s->scrollOffset);
		}
		if (s->selection >= s->scrollOffset && s->selection < s->scrollOffset + visible) {
			menu_render_row_full(m, s, s->selection, s->selection - s->scrollOffset);
		}
	}
}

static void menu_apply_delta(menu_row_t *row, int32_t delta) {
	if (row->kind == MENU_ROW_ICON_LIST) {
		if (!row->iconCount)
			return;
		int32_t idx = (int32_t) row->iconIndex + delta;
		int32_t n = (int32_t) row->iconCount;
		idx %= n;
		if (idx < 0)
			idx += n;
		row->iconIndex = (size_t) idx;
		return;
	}

	switch (row->typeinfo) {
	case MENU_FLOAT: {
		float v;
		memcpy(&v, row->value.ap, sizeof v);
		v += (float) delta * row->range.f.step;
		if (v < row->range.f.min)
			v = row->range.f.min;
		if (v > row->range.f.max)
			v = row->range.f.max;
		memcpy(row->value.ap, &v, sizeof v);
		break;
	}
	case MENU_INT: {
		int v;
		memcpy(&v, row->value.ap, sizeof v);
		int32_t nv = v + delta * row->range.i.step;
		if (nv < row->range.i.min)
			nv = row->range.i.min;
		if (nv > row->range.i.max)
			nv = row->range.i.max;
		v = (int) nv;
		memcpy(row->value.ap, &v, sizeof v);
		break;
	}
	case MENU_SHORT: {
		short v;
		memcpy(&v, row->value.ap, sizeof v);
		int32_t nv = (int32_t) v + delta * row->range.i.step;
		if (nv < row->range.i.min)
			nv = row->range.i.min;
		if (nv > row->range.i.max)
			nv = row->range.i.max;
		v = (short) nv;
		memcpy(row->value.ap, &v, sizeof v);
		break;
	}
	case MENU_U64: {
		uint64_t v;
		memcpy(&v, row->value.ap, sizeof v);
		int64_t nv = (int64_t) v + (int64_t) delta * row->range.i.step;
		if (nv < row->range.i.min)
			nv = row->range.i.min;
		if (nv > row->range.i.max)
			nv = row->range.i.max;
		v = (uint64_t) nv;
		memcpy(row->value.ap, &v, sizeof v);
		break;
	}
	default:
		break; // MENU_STRING isn't encoder-editable
	}
}

void menu_update(menu_t *m) {
	if (!m || m->screenCount == 0 || !m->navEncoder)
		return;

	int32_t navDelta = DRE_ReadStepDelta(m->navEncoder);
	bool navPressed = DRE_ReadButtonEdge(m->navEncoder);

	menu_screen_t *s = &m->screens[m->currentScreen];

	switch (m->focus) {
	case MENU_FOCUS_TABS:
		if (navDelta)
			menu_move_tab(m, navDelta);
		if (navPressed) {
			m->focus = MENU_FOCUS_ROWS;
			menu_render_tabs(m);
			menu_render_screen(m);
		}
		break;

	case MENU_FOCUS_ROWS:
		if (navDelta) {
			if (navDelta < 0 && s->selection == 0 && m->layout.tabBarHeight) {
				m->focus = MENU_FOCUS_TABS;
				menu_render_tabs(m);
				menu_render_screen(m);
			} else {
				menu_move_row(m, s, navDelta);
			}
		}
		if (navPressed && s->rowCount) {
			menu_row_t *row = &s->rows[s->selection];
			if (row->editable && m->inputMode == MENU_INPUT_PRESS_TO_EDIT) {
				m->focus = MENU_FOCUS_EDIT;
				row->_editing = true;
				menu_render_row_value_only(m, s, s->selection,
						s->selection - s->scrollOffset);
			}
		}
		if (m->inputMode == MENU_INPUT_DUAL_ENCODER && m->valueEncoder
				&& s->rowCount) {
			int32_t valDelta = DRE_ReadStepDelta(m->valueEncoder);
			if (valDelta) {
				menu_row_t *row = &s->rows[s->selection];
				if (row->editable) {
					menu_apply_delta(row, valDelta);
					menu_render_row_value_only(m, s, s->selection,
							s->selection - s->scrollOffset);
					if (row->onChange)
						row->onChange(row, row->ctx);
				}
			}
		}
		break;

	case MENU_FOCUS_EDIT: {
		menu_row_t *row = &s->rows[s->selection];
		if (navDelta) {
			menu_apply_delta(row, navDelta);
			menu_render_row_value_only(m, s, s->selection,
					s->selection - s->scrollOffset);
			if (row->onChange)
				row->onChange(row, row->ctx);
		}
		if (navPressed) {
			row->_editing = false;
			m->focus = MENU_FOCUS_ROWS;
			menu_render_row_value_only(m, s, s->selection,
					s->selection - s->scrollOffset);
			if (row->onCommit)
				row->onCommit(row, row->ctx);
		}
		break;
	}
	}
}

/* =========================================================================
 * Construction / public setters
 * ========================================================================= */

void menu_row_init_value(menu_row_t *row, const char *label,
		menu_value_type_e typeinfo, const void *initial, size_t size,
		int32_t min, int32_t max, int32_t step, const char *suffix,
		bool editable) {
	if (!row)
		return;
	memset(row, 0, sizeof(*row));
	row->label = label;
	row->kind = MENU_ROW_VALUE;
	row->typeinfo = typeinfo;
	row->editable = editable;
	row->suffix = suffix;
	row->range.i.min = min;
	row->range.i.max = max;
	row->range.i.step = step ? step : 1;
	any_make(initial, size, NULL, &row->value);
	row->fgColor = WHITE;
	row->bgColor = BLACK;
}

void menu_row_init_float(menu_row_t *row, const char *label, float initial,
		float min, float max, float step, const char *suffix,
		bool editable) {
	if (!row)
		return;
	memset(row, 0, sizeof(*row));
	row->label = label;
	row->kind = MENU_ROW_VALUE;
	row->typeinfo = MENU_FLOAT;
	row->editable = editable;
	row->suffix = suffix;
	row->range.f.min = min;
	row->range.f.max = max;
	row->range.f.step = (step != 0.0f) ? step : 1.0f;
	any_make(&initial, sizeof(initial), NULL, &row->value);
	row->fgColor = WHITE;
	row->bgColor = BLACK;
}

void menu_row_init_string(menu_row_t *row, const char *label,
		const char *initial) {
	if (!row)
		return;
	memset(row, 0, sizeof(*row));
	row->label = label;
	row->kind = MENU_ROW_VALUE;
	row->typeinfo = MENU_STRING;
	row->editable = false;
	size_t len = initial ? strlen(initial) : 0;
	if (len >= ANY_MAX_SZ)
		len = ANY_MAX_SZ - 1;
	any_make(initial, len, NULL, &row->value);
	row->fgColor = WHITE;
	row->bgColor = BLACK;
}

void menu_row_init_icon_list(menu_row_t *row, const char *label,
		const menu_icon_t *icons, size_t iconCount, size_t initialIndex,
		bool editable) {
	if (!row)
		return;
	memset(row, 0, sizeof(*row));
	row->label = label;
	row->kind = MENU_ROW_ICON_LIST;
	row->editable = editable;
	row->icons = icons;
	row->iconCount = iconCount;
	row->iconIndex = (iconCount && initialIndex < iconCount) ? initialIndex : 0;
	row->fgColor = WHITE;
	row->bgColor = BLACK;
}

void menu_row_set_colors(menu_row_t *row, uint16_t fgColor, uint16_t bgColor) {
	if (!row)
		return;
	row->fgColor = fgColor;
	row->bgColor = bgColor;
}

void menu_row_set_callbacks(menu_row_t *row,
		void (*onChange)(menu_row_t*, void*),
		void (*onCommit)(menu_row_t*, void*), void *ctx) {
	if (!row)
		return;
	row->onChange = onChange;
	row->onCommit = onCommit;
	row->ctx = ctx;
}

void menu_screen_init(menu_screen_t *screen, menu_row_t *rows, size_t rowCount,
		const menu_icon_t *tabIcon) {
	if (!screen)
		return;
	memset(screen, 0, sizeof(*screen));
	screen->rows = rows;
	screen->rowCount = rowCount;
	if (tabIcon)
		screen->tabIcon = *tabIcon;
}

menu_layout_t menu_layout_default(void) {
	menu_layout_t l =
			{ .font = Font_11x18, .rowHeight = 0, .rowPadX = 6,
					.rowSeparatorHeight = 2, .rowSeparatorColor = GRAY,
					.tabBarHeight = 40, .tabSize = 30, .tabGap = 4,
					.tabBarSeparatorHeight = 6, .tabBarSeparatorColor = GRAY,
					.tabBgColor = BLACK, .tabFgColor = WHITE,
					.tabActiveBgColor = LARS, .tabActiveFgColor = BLACK,
					.tabFocusBorderColor = WHITE, .editAccentColor = YELLOW,
					.bg = BLACK };

	return l;
}

void menu_init(menu_t *m, menu_screen_t *screens, size_t screenCount,
		menu_input_mode_e inputMode, dre_t *navEncoder, dre_t *valueEncoder,
		menu_layout_t *layout) {
	if (!m)
		return;

	menu_layout_t lay = layout ? *layout : menu_layout_default();

	*m = (menu_t ) { .screens = screens, .screenCount = screenCount,
					.currentScreen = 0, .tabScrollOffset = 0, .focus =
							(screenCount > 1 && lay.tabBarHeight) ?
									MENU_FOCUS_TABS : MENU_FOCUS_ROWS,
					.inputMode = inputMode, .navEncoder = navEncoder,
					.valueEncoder = valueEncoder, .layout = lay, };

	/* Prime the encoder(s) so the first menu_update() doesn't see a
	 * spurious jump from whatever the counter happened to be at boot. */
	if (navEncoder)
		(void) DRE_ReadStepDelta(navEncoder);
	if (valueEncoder)
		(void) DRE_ReadStepDelta(valueEncoder);
}

void menu_set_row_value(menu_t *m, size_t screenIndex, size_t rowIndex,
	const void *data, size_t size) {
if (!m || screenIndex >= m->screenCount)
	return;
menu_screen_t *s = &m->screens[screenIndex];
if (rowIndex >= s->rowCount || !data || size > ANY_MAX_SZ)
	return;

menu_row_t *row = &s->rows[rowIndex];
memcpy(row->value.buf, data, size);
row->value.size = size;
row->value.ap = row->value.buf;

if (screenIndex == m->currentScreen && rowIndex >= s->scrollOffset
		&& rowIndex < s->scrollOffset + menu_visible_rows(m, s)) {
	menu_render_row_value_only(m, s, rowIndex, rowIndex - s->scrollOffset);
}
}

void menu_set_row_icon_index(menu_t *m, size_t screenIndex, size_t rowIndex,
	size_t iconIndex) {
if (!m || screenIndex >= m->screenCount)
	return;
menu_screen_t *s = &m->screens[screenIndex];
if (rowIndex >= s->rowCount)
	return;

menu_row_t *row = &s->rows[rowIndex];
if (row->kind != MENU_ROW_ICON_LIST || iconIndex >= row->iconCount)
	return;
row->iconIndex = iconIndex;

if (screenIndex == m->currentScreen && rowIndex >= s->scrollOffset
		&& rowIndex < s->scrollOffset + menu_visible_rows(m, s)) {
	menu_render_row_value_only(m, s, rowIndex, rowIndex - s->scrollOffset);
}
}
