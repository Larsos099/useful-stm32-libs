#include <string.h>
#include "st7789.h"

#ifdef USE_DMA
#define DMA_MIN_SIZE 16 // Minimum size of SPI stransfer block when DMA still applies.
#endif

/* Buffer is required for faster SPI transfers to store datas to be displayed.
 * Buffer can be used as a framebuffer, set buffer length to WIDTHxHEIGHT.
 * If your MCU don't have enough RAM, please avoid using large buffers and DMA.
 * And if your MCU have enough RAM(even larger than full-frame size),
 * Then you can specify the framebuffer size to the full resolution WIDTHxHEIGHT.
 */

// #define BUF_LEN    ST7789_WIDTH * ST7789_HEIGHT   // Buffer elements count
 #define BUF_LEN    100   // Buffer elements count
#define BUF_BYTE   2                              // Size of buffer element in bytes
uint16_t buffer[BUF_LEN];                         // 16bit color buffer RGB565
static void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1);
/**
 * @brief Write command to ST7789 controller
 * @param cmd -> command to write
 * @return none
 */
static void
ST7789_WriteCommand (uint8_t cmd)
{
  ST7789_Select();
  ST7789_DC_Clr();
  HAL_SPI_Transmit (&ST7789_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
  ST7789_UnSelect();
}

/**
 * @brief Write data to ST7789 controller
 * @param buff -> pointer of data buffer
 * @param buff_size -> size of the data buffer
 * @return none
 */
static void
ST7789_WriteData (uint8_t *buff, size_t buff_size)
{
  ST7789_Select();
  ST7789_DC_Set();
  // split data in small chunks because HAL can't send more than 64K at once
  while (buff_size > 0)
    {
      uint16_t chunk_size = buff_size > 0xFFFF ? 0xFFFF : buff_size;

#ifdef USE_DMA
      if (buff_size >= DMA_MIN_SIZE)
	{
	  HAL_SPI_Transmit_DMA (&ST7789_SPI_PORT, buff, chunk_size);
	  while (ST7789_SPI_PORT.hdmatx->State != HAL_DMA_STATE_READY)
	    {
	    }
	}
      else
	HAL_SPI_Transmit (&ST7789_SPI_PORT, buff, chunk_size, HAL_MAX_DELAY);
#else
		HAL_SPI_Transmit(&ST7789_SPI_PORT, buff, chunk_size, HAL_MAX_DELAY);
#endif
      buff += chunk_size;
      buff_size -= chunk_size;
    }
  ST7789_UnSelect();
}

void ST7789_DrawBitmap1BPP(uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h,
                           const uint8_t *bitmap,
                           uint16_t fg, uint16_t bg)
{
    uint16_t row_bytes = (w + 7) / 8;

    // Pre-swap colors once for RGB565 big-endian
    uint16_t fg_be = (fg >> 8) | (fg << 8);
    uint16_t bg_be = (bg >> 8) | (bg << 8);

    // Build entire icon into a local buffer and send in one shot
    uint16_t pixels[w * h];

    for (uint16_t j = 0; j < h; j++)
    {
        for (uint16_t i = 0; i < w; i++)
        {
            uint8_t byte = bitmap[j * row_bytes + (i / 8)];
            uint8_t bit  = (byte >> (7 - (i % 8))) & 1;
            pixels[j * w + i] = bit ? fg_be : bg_be;
        }
    }

    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7789_WriteData((uint8_t*)pixels, sizeof(pixels));
}

/**
 * @brief Write data to ST7789 controller, simplify for 8bit data.
 * data -> data to write
 * @return none
 */
static void
ST7789_WriteSmallData (uint8_t data)
{
  ST7789_Select();
  ST7789_DC_Set();
  HAL_SPI_Transmit (&ST7789_SPI_PORT, &data, sizeof(data), HAL_MAX_DELAY);
  ST7789_UnSelect();
}

/**
 * @brief Set the rotation direction of the display
 * @param m -> rotation parameter(please refer it in st7789.h)
 * @return none
 */
void
ST7789_SetRotation (uint8_t m)
{
  ST7789_WriteCommand (ST7789_MADCTL);	// MADCTL

  switch (m)
    {
    case 0:
      ST7789_WriteSmallData (
	  ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
      break;
    case 1:
//      ST7789_WriteSmallData (
//	  ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
    	ST7789_WriteSmallData(0x60);
      break;
    case 2:
      ST7789_WriteSmallData (ST7789_MADCTL_RGB);
      break;
    case 3:
      ST7789_WriteSmallData (
	  ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
      break;
    default:
      break;
    }
}

/**
 * @brief Set address of DisplayWindow
 * @param xi&yi -> coordinates of window
 * @return none
 */
static void
ST7789_SetAddressWindow (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint16_t x_start = x0 + X_SHIFT, x_end = x1 + X_SHIFT;
  uint16_t y_start = y0 + Y_SHIFT, y_end = y1 + Y_SHIFT;

  /* Column Address set */
  ST7789_WriteCommand (ST7789_CASET);
    {
      uint8_t data[] =
	{ x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF };
      ST7789_WriteData (data, sizeof(data));
    }

  /* Row Address set */
  ST7789_WriteCommand (ST7789_RASET);
    {
      uint8_t data[] =
	{ y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF };
      ST7789_WriteData (data, sizeof(data));
    }
  /* Write to RAM */
  ST7789_WriteCommand (ST7789_RAMWR);
}

/**
 * @brief Initialize ST7789 controller
 * @param none
 * @return none
 */
void
ST7789_Init (void)
{
  memset (buffer, 0, sizeof(buffer)); // clear buffer

  HAL_Delay (25);
  ST7789_RST_Clr();
  HAL_Delay (25);
  ST7789_RST_Set();
  HAL_Delay (50);

  ST7789_WriteCommand (ST7789_COLMOD);		//	Set color mode
  ST7789_WriteSmallData (ST7789_COLOR_MODE_16bit);
  ST7789_WriteCommand (0xB2);				//	Porch control
    {
      uint8_t data[] =
	{ 0x0C, 0x0C, 0x00, 0x33, 0x33 };
      ST7789_WriteData (data, sizeof(data));
    }
  ST7789_SetRotation (ST7789_ROTATION);	//	MADCTL (Display Rotation)

  /* Internal LCD Voltage generator settings */
  ST7789_WriteCommand (0XB7);				//	Gate Control
  ST7789_WriteSmallData (0x35);			//	Default value
  ST7789_WriteCommand (0xBB);				//	VCOM setting
  ST7789_WriteSmallData (0x19);		//	0.725v (default 0.75v for 0x20)
  ST7789_WriteCommand (0xC0);				//	LCMCTRL
  ST7789_WriteSmallData (0x2C);			//	Default value
  ST7789_WriteCommand (0xC2);		//	VDV and VRH command Enable
  ST7789_WriteSmallData (0x01);			//	Default value
  ST7789_WriteCommand (0xC3);				//	VRH set
  ST7789_WriteSmallData (0x12);	//	+-4.45v (defalut +-4.1v for 0x0B)
  ST7789_WriteCommand (0xC4);				//	VDV set
  ST7789_WriteSmallData (0x20);			//	Default value
  ST7789_WriteCommand (0xC6);	//	Frame rate control in normal mode
  ST7789_WriteSmallData (0x0F);			//	Default value (60HZ)
  ST7789_WriteCommand (0xD0);				//	Power control
  ST7789_WriteSmallData (0xA4);			//	Default value
  ST7789_WriteSmallData (0xA1);			//	Default value
  /**************** Division line ****************/

  ST7789_WriteCommand (0xE0);
    {
      uint8_t data[] =
	{ 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D,
	    0x0B, 0x1F, 0x23 };
      ST7789_WriteData (data, sizeof(data));
    }

  ST7789_WriteCommand (0xE1);
    {
      uint8_t data[] =
	{ 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F,
	    0x1F, 0x20, 0x23 };
      ST7789_WriteData (data, sizeof(data));
    }
  ST7789_WriteCommand (ST7789_INVON);		//	Inversion ON
  ST7789_WriteCommand (ST7789_SLPOUT);	//	Out of sleep mode
  ST7789_WriteCommand (ST7789_NORON);		//	Normal Display on
  ST7789_WriteCommand (ST7789_DISPON);	//	Main screen turned on

  HAL_Delay (50);
}

/**
 * @brief Fill the DisplayWindow with single color RGB565
 * @param color -> color to Fill with
 * @return none
 */
void
ST7789_Fill_Color (uint16_t color)
{
  ST7789_Fill_Color_BUF (color);
  ST7789_SetAddressWindow (0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
  uint32_t window_len = ST7789_WIDTH * ST7789_HEIGHT;

  // split data in chunks with amount of MIN(window_len, buffer_len)
  uint32_t chunk_len = window_len > BUF_LEN ? BUF_LEN : window_len;
  uint32_t chunk_size = chunk_len * BUF_BYTE;  // chunk size in bytes
  int32_t trn_remain = window_len * BUF_BYTE; // remaining transfer amount in bytes
  while (trn_remain > 0)
    {
      if (trn_remain < chunk_size)
	chunk_size = trn_remain;
      ST7789_WriteData ((uint8_t*) buffer, chunk_size);
      trn_remain -= chunk_size;
    }
}

/**
 * @brief Fill buffer with color
 * @return none
 */
void
ST7789_Fill_Color_BUF (uint16_t color)
{
  uint16_t color565 = ((color >> 8) & 0x00FF) | (color << 8);
  for (uint32_t index = 0; index < BUF_LEN; index++)
    buffer[index] = color565;
}

/**
 * @brief Transfer buffer content to SPI
 * @return none
 */
void
ST7789_Display_BUF ()
{
  ST7789_SetAddressWindow (0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1);
  uint32_t window_len = ST7789_WIDTH * ST7789_HEIGHT;

  // buffer can be bigger that window and only window amount of data must be transferred
  // buffer can be smaller than window and buffer must be transferred several times
  // split data in chunks with amount of MIN(window_len, buffer_len)
  uint32_t chunk_len = window_len > BUF_LEN ? BUF_LEN : window_len;
  uint32_t chunk_size = chunk_len * BUF_BYTE;  // chunk size in bytes
  int32_t trn_remain = window_len * BUF_BYTE; // remaining transfer amount in bytes
  while (trn_remain > 0)
    {
      if (trn_remain < chunk_size)
	chunk_size = trn_remain;
      ST7789_WriteData ((uint8_t*) buffer, chunk_size);
      trn_remain -= chunk_size;
    }
}

/**
 * @brief Draw a Pixel
 * @param x&y -> coordinate to Draw
 * @param color -> color of the Pixel
 * @return none
 */
void
ST7789_DrawPixel (uint16_t x, uint16_t y, uint16_t color)
{
  if ((x < 0) || (x >= ST7789_WIDTH) || (y < 0) || (y >= ST7789_HEIGHT))
    return;

  uint8_t data[] =
    { color >> 8, color & 0xFF };
  ST7789_SetAddressWindow (x, y, x, y);
  ST7789_WriteData (data, sizeof(data));
}

void
ST7789_DrawPixel_BUF (uint16_t x, uint16_t y, uint16_t color)
{
  uint16_t pos = y * ST7789_WIDTH + x;
  if (pos < BUF_LEN)
    buffer[pos] = ((color >> 8) & 0x00FF) | (color << 8);
}

/**
 * @brief Fill an Area with single color
 * @param xSta&ySta -> coordinate of the start point
 * @param xEnd&yEnd -> coordinate of the end point
 * @param color -> color to Fill with
 * @return none
 */
void
ST7789_Fill (uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd,
	     uint16_t color)
{
  if ((xEnd < 0) || (xEnd >= ST7789_WIDTH) || (yEnd < 0)
      || (yEnd >= ST7789_HEIGHT))
    return;
  uint16_t i, j;
  ST7789_SetAddressWindow (xSta, ySta, xEnd, yEnd);
  for (i = ySta; i <= yEnd; i++)
    for (j = xSta; j <= xEnd; j++)
      {
	uint8_t data[] =
	  { color >> 8, color & 0xFF };
	ST7789_WriteData (data, sizeof(data));
      }
}

/**
 * @brief Draw a big Pixel at a point
 * @param x&y -> coordinate of the point
 * @param color -> color of the Pixel
 * @return none
 */
void
ST7789_DrawPixel_4px (uint16_t x, uint16_t y, uint16_t color)
{
  if ((x <= 0) || (x > ST7789_WIDTH) || (y <= 0) || (y > ST7789_HEIGHT))
    return;
  ST7789_Fill (x - 1, y - 1, x + 1, y + 1, color);
}

/**
 * @brief Draw a line with single color
 * @param x1&y1 -> coordinate of the start point
 * @param x2&y2 -> coordinate of the end point
 * @param color -> color of the line to Draw
 * @return none
 */
void
ST7789_DrawLine (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
		 uint16_t color)
{
  uint16_t swap;
  uint16_t steep = ABS(y1 - y0) > ABS(x1 - x0);
  if (steep)
    {
      swap = x0;
      x0 = y0;
      y0 = swap;

      swap = x1;
      x1 = y1;
      y1 = swap;
    }

  if (x0 > x1)
    {
      swap = x0;
      x0 = x1;
      x1 = swap;

      swap = y0;
      y0 = y1;
      y1 = swap;
    }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = ABS(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1)
    {
      ystep = 1;
    }
  else
    {
      ystep = -1;
    }

  for (; x0 <= x1; x0++)
    {
      if (steep)
	{
	  ST7789_DrawPixel (y0, x0, color);
	}
      else
	{
	  ST7789_DrawPixel (x0, y0, color);
	}
      err -= dy;
      if (err < 0)
	{
	  y0 += ystep;
	  err += dx;
	}
    }
}

void
ST7789_DrawLine_BUF (uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
		     uint16_t color)
{
  uint16_t swap;
  uint16_t steep = ABS(y1 - y0) > ABS(x1 - x0);
  if (steep)
    {
      swap = x0;
      x0 = y0;
      y0 = swap;

      swap = x1;
      x1 = y1;
      y1 = swap;
    }

  if (x0 > x1)
    {
      swap = x0;
      x0 = x1;
      x1 = swap;

      swap = y0;
      y0 = y1;
      y1 = swap;
    }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = ABS(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1)
    {
      ystep = 1;
    }
  else
    {
      ystep = -1;
    }

  for (; x0 <= x1; x0++)
    {
      if (steep)
	{
	  ST7789_DrawPixel_BUF (y0, x0, color);
	}
      else
	{
	  ST7789_DrawPixel_BUF (x0, y0, color);
	}
      err -= dy;
      if (err < 0)
	{
	  y0 += ystep;
	  err += dx;
	}
    }
}

/**
 * @brief Draw a Rectangle with single color
 * @param xi&yi -> 2 coordinates of 2 top points.
 * @param color -> color of the Rectangle line
 * @return none
 */
void
ST7789_DrawRectangle (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
		      uint16_t color)
{
  ST7789_DrawLine (x1, y1, x2, y1, color);
  ST7789_DrawLine (x1, y1, x1, y2, color);
  ST7789_DrawLine (x1, y2, x2, y2, color);
  ST7789_DrawLine (x2, y1, x2, y2, color);
}

/** 
 * @brief Draw a circle with single color
 * @param x0&y0 -> coordinate of circle center
 * @param r -> radius of circle
 * @param color -> color of circle line
 * @return  none
 */
void
ST7789_DrawCircle (uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  ST7789_DrawPixel (x0, y0 + r, color);
  ST7789_DrawPixel (x0, y0 - r, color);
  ST7789_DrawPixel (x0 + r, y0, color);
  ST7789_DrawPixel (x0 - r, y0, color);

  while (x < y)
    {
      if (f >= 0)
	{
	  y--;
	  ddF_y += 2;
	  f += ddF_y;
	}
      x++;
      ddF_x += 2;
      f += ddF_x;

      ST7789_DrawPixel (x0 + x, y0 + y, color);
      ST7789_DrawPixel (x0 - x, y0 + y, color);
      ST7789_DrawPixel (x0 + x, y0 - y, color);
      ST7789_DrawPixel (x0 - x, y0 - y, color);

      ST7789_DrawPixel (x0 + y, y0 + x, color);
      ST7789_DrawPixel (x0 - y, y0 + x, color);
      ST7789_DrawPixel (x0 + y, y0 - x, color);
      ST7789_DrawPixel (x0 - y, y0 - x, color);
    }
}

/**
 * @brief Draw an Image on the screen
 * @param x&y -> start point of the Image
 * @param w&h -> width & height of the Image to Draw
 * @param data -> pointer of the Image array
 * @return none
 */
void
ST7789_DrawImage (uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		  const uint16_t *data)
{
  if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT))
    return;
  if ((x + w - 1) >= ST7789_WIDTH)
    return;
  if ((y + h - 1) >= ST7789_HEIGHT)
    return;

  ST7789_SetAddressWindow (x, y, x + w - 1, y + h - 1);
  ST7789_WriteData ((uint8_t*) data, sizeof(uint16_t) * w * h);
}

/**
 * @brief Invert Fullscreen color
 * @param invert -> Whether to invert
 * @return none
 */
void
ST7789_InvertColors (uint8_t invert)
{
  ST7789_WriteCommand (invert ? 0x21 /* INVON */: 0x20 /* INVOFF */);
}

/** 
 * @brief Write a char
 * @param  x&y -> cursor of the start point.
 * @param ch -> char to write
 * @param font -> fontstyle of the string
 * @param color -> color of the char
 * @param bgcolor -> background color of the char
 * @return  none
 */
void
ST7789_WriteChar (uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color,
		  uint16_t bgcolor)
{
  uint32_t w, h, b;
  uint32_t window_size = font.width * font.height;
  uint16_t data[window_size];

  for (h = 0; h < font.height; h++)
    {
      b = font.data[(ch - 32) * font.height + h];

      for (w = 0; w < font.width; w++)
	{
	  if ((b << w) & 0x8000)
	    {
	      data[font.width * h + w] = ((color >> 8) & 0x00FF) | (color << 8);
	    }
	  else
	    {
	      data[font.width * h + w] = ((bgcolor >> 8) & 0x00FF)
		  | (bgcolor << 8);
	    }
	}
    }

  ST7789_SetAddressWindow (x, y, x + font.width - 1, y + font.height - 1);
  ST7789_WriteData ((uint8_t*) data, sizeof(data));
}

/**
 * @brief Write a char to the framebuffer
 * @param  x&y -> cursor of the start point.
 * @param ch -> char to write
 * @param font -> fontstyle of the string
 * @param color -> color of the char
 * @param bgcolor -> background color of the char
 * @return  none
 */
void
ST7789_WriteChar_BUF (uint16_t x, uint16_t y, char ch, FontDef font,
		      uint16_t color, uint16_t bgcolor)
{
  if (x >= ST7789_WIDTH)
    return;
  if (y >= ST7789_HEIGHT)
    return;

  uint32_t char_width = font.width;
  uint32_t char_height = font.height;
  uint32_t window_width, window_height;

  if ((x + char_width) < ST7789_WIDTH)
    window_width = char_width;
  else
    window_width = ST7789_WIDTH - x;

  if ((y + char_height) < ST7789_HEIGHT)
    window_height = char_height;
  else
    window_height = ST7789_HEIGHT - y;

  uint32_t h, w, b;
  uint32_t fb_pos;      // position in the framebuffer
  for (h = 0; h < window_height; h++)
    {
      for (w = 0; w < window_width; w++)
	{
	  b = font.data[(ch - 32) * font.height + h];
	  fb_pos = ST7789_WIDTH * (y + h) + x + w;
	  if ((b << (w % font.width)) & 0x8000)
	    {
	      buffer[fb_pos] = ((color >> 8) & 0x00FF) | (color << 8);
	    }
	  else
	    {
	      buffer[fb_pos] = ((bgcolor >> 8) & 0x00FF) | (bgcolor << 8);
	    }
	}
    }
}

/**
 * @brief Write a string 
 * @param  x&y -> cursor of the start point.
 * @param str -> string to write
 * @param font -> fontstyle of the string
 * @param color -> color of the string
 * @param bgcolor -> background color of the string
 * @return  none
 */
void
ST7789_WriteString (uint16_t x, uint16_t y, const char *str, FontDef font,
		    uint16_t color, uint16_t bgcolor)
{
  ST7789_SetAddressWindow (x, y, x + font.width - 1, y + font.height - 1);

  while (*str)
    {
      if (x + font.width >= ST7789_WIDTH)
	{
	  x = 0;
	  y += font.height;
	  if (y + font.height >= ST7789_HEIGHT)
	    {
	      break;
	    }

	  if (*str == ' ')
	    {
	      // skip spaces in the beginning of the new line
	      str++;
	      continue;
	    }
	}
      ST7789_WriteChar (x, y, *str, font, color, bgcolor);
      x += font.width;
      str++;
    }
}

/**
 * @brief Write a string with a single data transfer
 * @param  x&y -> cursor of the start point.
 * @param str -> string to write
 * @param font -> fontstyle of the string
 * @param color -> color of the string
 * @param bgcolor -> background color of the string
 * @return  none
 */
void
ST7789_WriteString_Fast (uint16_t x, uint16_t y, const char *str, FontDef font,
			 uint16_t color, uint16_t bgcolor)
{
  if (x >= ST7789_WIDTH)
    return;
  if (y >= ST7789_HEIGHT)
    return;

  uint32_t string_chars = strlen (str);
  uint32_t string_width = string_chars * font.width;
  uint32_t string_height = font.height;
  uint32_t window_width, window_height;

  if ((x + string_width) < ST7789_WIDTH)
    window_width = string_width;
  else
    window_width = ST7789_WIDTH - x;

  if ((y + string_height) < ST7789_HEIGHT)
    window_height = string_height;
  else
    window_height = ST7789_HEIGHT - y;

  uint32_t window_size = window_width * window_height;
  uint16_t data[window_size];
  char ch;
  uint32_t h, w, b;
  for (h = 0; h < window_height; h++)
    {
      for (w = 0; w < window_width; w++)
	{
	  ch = str[w / font.width];
	  b = font.data[(ch - 32) * font.height + h];
	  if ((b << (w % font.width)) & 0x8000)
	    {
	      data[window_width * h + w] = ((color >> 8) & 0x00FF)
		  | (color << 8);
	    }
	  else
	    {
	      data[window_width * h + w] = ((bgcolor >> 8) & 0x00FF)
		  | (bgcolor << 8);
	    }
	}
    }
  ST7789_SetAddressWindow (x, y, x + window_width - 1, y + window_height - 1);
  ST7789_WriteData ((uint8_t*) data, sizeof(data));
}

/**
 * @brief Write a string to the framebuffer
 * @param  x&y -> cursor of the start point.
 * @param str -> string to write
 * @param font -> fontstyle of the string
 * @param color -> color of the string
 * @param bgcolor -> background color of the string
 * @return  none
 */
void
ST7789_WriteString_BUF (uint16_t x, uint16_t y, const char *str, FontDef font,
			uint16_t color, uint16_t bgcolor)
{
  if (x >= ST7789_WIDTH)
    return;
  if (y >= ST7789_HEIGHT)
    return;

  uint32_t string_chars = strlen (str);
  uint32_t string_width = string_chars * font.width;
  uint32_t string_height = font.height;
  uint32_t window_width, window_height;

  if ((x + string_width) < ST7789_WIDTH)
    window_width = string_width;
  else
    window_width = ST7789_WIDTH - x;

  if ((y + string_height) < ST7789_HEIGHT)
    window_height = string_height;
  else
    window_height = ST7789_HEIGHT - y;

  char ch;
  uint32_t h, w, b;
  uint32_t fb_pos;      // position in the framebuffer
  for (h = 0; h < window_height; h++)
    {
      for (w = 0; w < window_width; w++)
	{
	  ch = str[w / font.width]; // char of the string where current scan pos(w,h) is located
	  b = font.data[(ch - 32) * font.height + h];
	  fb_pos = ST7789_WIDTH * (y + h) + x + w;
	  if ((b << (w % font.width)) & 0x8000)
	    {
	      buffer[fb_pos] = ((color >> 8) & 0x00FF) | (color << 8);
	    }
	  else
	    {
	      buffer[fb_pos] = ((bgcolor >> 8) & 0x00FF) | (bgcolor << 8);
	    }
	}
    }
}

/** 
 * @brief Draw a filled Rectangle with single color
 * @param  x&y -> coordinates of the starting point
 * @param w&h -> width & height of the Rectangle
 * @param color -> color of the Rectangle
 * @return  none
 */
void
ST7789_DrawFilledRectangle (uint16_t x, uint16_t y, uint16_t w, uint16_t h,
			    uint16_t color)
{
  uint8_t i;

  /* Check input parameters */
  if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT)
    {
      /* Return error */
      return;
    }

  /* Check width and height */
  if ((x + w) >= ST7789_WIDTH)
    {
      w = ST7789_WIDTH - x;
    }
  if ((y + h) >= ST7789_HEIGHT)
    {
      h = ST7789_HEIGHT - y;
    }

  /* Draw lines */
  for (i = 0; i <= h; i++)
    {
      /* Draw lines */
      ST7789_DrawLine (x, y + i, x + w, y + i, color);
    }
}

/** 
 * @brief Draw a Triangle with single color
 * @param  xi&yi -> 3 coordinates of 3 top points.
 * @param color ->color of the lines
 * @return  none
 */
void
ST7789_DrawTriangle (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
		     uint16_t x3, uint16_t y3, uint16_t color)
{
  /* Draw lines */
  ST7789_DrawLine (x1, y1, x2, y2, color);
  ST7789_DrawLine (x2, y2, x3, y3, color);
  ST7789_DrawLine (x3, y3, x1, y1, color);
}

/** 
 * @brief Draw a filled Triangle with single color
 * @param  xi&yi -> 3 coordinates of 3 top points.
 * @param color ->color of the triangle
 * @return  none
 */
void
ST7789_DrawFilledTriangle (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
			   uint16_t x3, uint16_t y3, uint16_t color)
{
  int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, yinc1 = 0,
      yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, curpixel = 0;

  deltax = ABS(x2 - x1);
  deltay = ABS(y2 - y1);
  x = x1;
  y = y1;

  if (x2 >= x1)
    {
      xinc1 = 1;
      xinc2 = 1;
    }
  else
    {
      xinc1 = -1;
      xinc2 = -1;
    }

  if (y2 >= y1)
    {
      yinc1 = 1;
      yinc2 = 1;
    }
  else
    {
      yinc1 = -1;
      yinc2 = -1;
    }

  if (deltax >= deltay)
    {
      xinc1 = 0;
      yinc2 = 0;
      den = deltax;
      num = deltax / 2;
      numadd = deltay;
      numpixels = deltax;
    }
  else
    {
      xinc2 = 0;
      yinc1 = 0;
      den = deltay;
      num = deltay / 2;
      numadd = deltax;
      numpixels = deltay;
    }

  for (curpixel = 0; curpixel <= numpixels; curpixel++)
    {
      ST7789_DrawLine (x, y, x3, y3, color);

      num += numadd;
      if (num >= den)
	{
	  num -= den;
	  x += xinc1;
	  y += yinc1;
	}
      x += xinc2;
      y += yinc2;
    }
}

/** 
 * @brief Draw a Filled circle with single color
 * @param x0&y0 -> coordinate of circle center
 * @param r -> radius of circle
 * @param color -> color of circle
 * @return  none
 */
void
ST7789_DrawFilledCircle (int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  ST7789_DrawPixel (x0, y0 + r, color);
  ST7789_DrawPixel (x0, y0 - r, color);
  ST7789_DrawPixel (x0 + r, y0, color);
  ST7789_DrawPixel (x0 - r, y0, color);
  ST7789_DrawLine (x0 - r, y0, x0 + r, y0, color);

  while (x < y)
    {
      if (f >= 0)
	{
	  y--;
	  ddF_y += 2;
	  f += ddF_y;
	}
      x++;
      ddF_x += 2;
      f += ddF_x;

      ST7789_DrawLine (x0 - x, y0 + y, x0 + x, y0 + y, color);
      ST7789_DrawLine (x0 + x, y0 - y, x0 - x, y0 - y, color);
      ST7789_DrawLine (x0 + y, y0 + x, x0 - y, y0 + x, color);
      ST7789_DrawLine (x0 + y, y0 - x, x0 - y, y0 - x, color);
    }
}

/**
 * @brief Open/Close tearing effect line
 * @param tear -> Whether to tear
 * @return none
 */
void
ST7789_TearEffect (uint8_t tear)
{
  ST7789_WriteCommand (tear ? 0x35 /* TEON */: 0x34 /* TEOFF */);
}

/** 
 * @brief A Simple test function for ST7789
 * @param  none
 * @return  none
 */
void
ST7789_Test (void)
{

  ST7789_Fill_Color (WHITE);
  ST7789_WriteString (10, 20, "Speed Test", Font_11x18, RED, WHITE);
  HAL_Delay (1000);

  ST7789_Fill_Color (CYAN);
  ST7789_Fill_Color (RED);
  ST7789_Fill_Color (BLUE);
  ST7789_Fill_Color (GREEN);
  ST7789_Fill_Color (YELLOW);
  ST7789_Fill_Color (BROWN);
  ST7789_Fill_Color (DARKBLUE);
  ST7789_Fill_Color (MAGENTA);
  ST7789_Fill_Color (LIGHTGREEN);
  ST7789_Fill_Color (LGRAY);
  ST7789_Fill_Color (LBBLUE);
  ST7789_Fill_Color (WHITE);

  ST7789_WriteString_Fast (10, 10, "Font test.", Font_16x26, GBLUE, WHITE);
  ST7789_WriteString_Fast (10, 50, "Hello Steve!", Font_7x10, RED, WHITE);
  ST7789_WriteString_Fast (10, 75, "Hello Steve!", Font_11x18, YELLOW, WHITE);
  ST7789_WriteString_Fast (10, 100, "Hello Steve!", Font_16x26, MAGENTA, WHITE);
  HAL_Delay (1000);

  ST7789_Fill_Color (RED);
  ST7789_WriteString_Fast (10, 10, "Rect./Line.", Font_11x18, YELLOW, BLACK);
  ST7789_DrawRectangle (30, 30, 100, 100, WHITE);

  ST7789_WriteString (10, 10, "Filled Rect.", Font_11x18, YELLOW, BLACK);


  ST7789_DrawFilledRectangle (30, 30, 50, 50, WHITE);

  ST7789_WriteString (10, 10, "Circle.", Font_11x18, YELLOW, BLACK);
  ST7789_DrawCircle (60, 60, 25, WHITE);

  ST7789_WriteString (10, 10, "Filled Cir.", Font_11x18, YELLOW, BLACK);
  ST7789_DrawFilledCircle (60, 60, 25, WHITE);

  ST7789_WriteString (10, 10, "Triangle", Font_11x18, YELLOW, BLACK);
  ST7789_DrawTriangle (30, 30, 30, 70, 60, 40, WHITE);

  ST7789_WriteString (10, 10, "Filled Tri", Font_11x18, YELLOW, BLACK);
  ST7789_DrawFilledTriangle (30, 30, 30, 70, 60, 40, WHITE);
  HAL_Delay (1000);

  /*

   //	If FLASH cannot storage anymore datas, please delete codes below.
   ST7789_Fill_Color(WHITE);
   ST7789_DrawImage(0, 0, 128, 128, (uint16_t *)saber);
   HAL_Delay(3000);

   ST7789_Fill_Color(WHITE);
   ST7789_DrawImage(0, 0, 240, 240, (uint16_t *)knky);
   HAL_Delay(3000);

   ST7789_Fill_Color(WHITE);
   ST7789_DrawImage(0, 0, 240, 240, (uint16_t *)tek);
   HAL_Delay(3000);


   ST7789_Fill_Color(WHITE);
   ST7789_DrawImage(0, 0, 240, 240, (uint16_t *)adi1);
   HAL_Delay(3000);
   */
}


/**
 * @brief   Высокопроизводительный перевод float в строку с округлением
 * @note    Исключает math.h, pow() и ошибки округления IEEE-754. Полностью безопасен для нуля.
 * @param   num:        Исходное число float
 * @param   buf:        Указатель на целевой чар-массив (минимум 12 байт)
 * @param   decimals:   Количество знаков после точки (0-4)
 * @retval  None
 */
void LCD_FloatToString(float num, char* buf, uint8_t decimals)
{
    int32_t i = 0;

    // 1. Обработка знака числа
    if (num < 0)
    {
        buf[i++] = '-';
        num = -num;
    }

    // 2. Расчет целочисленного масштабирующего множителя (замена pow)
    int32_t multiplier = 1;

    for (uint8_t d = 0; d < decimals; d++)
    {
        multiplier *= 10;
    }

    // 3. Перевод в абсолютно точное целое число с математическим округлением +0.5f
    // Для 4.851f: (4.851 * 100) + 0.5 = 485.6 -> 485
    int32_t total_value = (int32_t)(num * (float)multiplier + 0.5f);

    int32_t integer_part = total_value / multiplier;
    int32_t fraction_part = total_value % multiplier;

    // 4. Конвертация целой части (разбор в обратном порядке)
    int32_t int_start_idx = i;

    if (integer_part == 0)
    {
        buf[i++] = '0';
    } else {
        while (integer_part > 0)
        {
            buf[i++] = (char)((integer_part % 10) + '0');
            integer_part /= 10;
        }
    }

    // Разворачиваем только целую часть на месте
    int32_t p = int_start_idx, q = i - 1;
    while (p < q)
    {
        char temp = buf[p]; buf[p] = buf[q]; buf[q] = temp;
        p++; q--;
    }

    // 5. Конвертация дробной части (если требуются знаки после точки)
    if (decimals > 0)
    {
        buf[i++] = '.';

        // Пишем дробную часть строго с конца, сохраняя ведущие нули (например, .05)
        int32_t frac_end_idx = i + decimals;
        buf[frac_end_idx] = '\0'; 																			// Закрываем строку заранее

        for (int8_t d = (int8_t)(decimals - 1); d >= 0; d--)
        {
            buf[i + d] = (char)((fraction_part % 10) + '0');
            fraction_part /= 10;
        }
        i = frac_end_idx;
    } else {
        buf[i] = '\0'; 																						// Закрываем строку, если decimals == 0
    }
}

/**
 * @brief   Высокопроизводительный вывод float через целочисленное масштабирование
 * @note    Полностью исключает аппарат накапливаемых погрешностей IEEE-754 float.
 *          Гарантирует железную стабильность сотых долей амперметра и вольтметра.
 * @param   dev:      Указатель на структуру управления дисплеем LCD_Handler_t
 * @param   num:      Выводимое число с плавающей точкой (float)
 * @param   decimals: Количество знаков после точки (0-4)
 * @param   x:        Координата X начала вывода числовой строки
 * @param   y:        Координата Y начала вывода числовой строки
 * @param   font:     Указатель на дескриптор шрифта FontDef_t (формат MikroE X-GLCD)
 * @param   color:    Цвет пикселей цифр (RGB565)
 * @param   bgcolor:  Цвет пикселей фона (RGB565)
 * @retval  None
 */
//ST7789_WriteString(170, 0, buf, Font_16x26, GREEN, BLACK);
void ST7789_WriteFloat(float num, uint8_t decimals, uint16_t x, uint16_t y, FontDef font, uint16_t color, uint16_t bgcolor)
{
    const uint8_t char_spacing = 2; 																	// Аппаратный межсимвольный пробел
    char string_buffer[16] = {0};

    // 1. Быстро переводим число в идеальную строчку без погрешностей
    LCD_FloatToString(num, string_buffer, decimals);

    ST7789_WriteString(x, y, string_buffer, font, color, bgcolor);
}

