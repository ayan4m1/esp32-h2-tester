#ifndef DISPLAY_H
#define DISPLAY_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "display_config.h"
// BEGIN NON-USER CODE
#define LCD_COLOR_RGB 1
#define LCD_COLOR_BGR 2
#define LCD_COLOR_GSC 3

#ifdef LCD_PIN_NUM_BCKL
#ifndef LCD_BCKL_ON_LEVEL
#define LCD_BCKL_ON_LEVEL 1
#endif
#ifndef LCD_BCKL_OFF_LEVEL
#define LCD_BCKL_OFF_LEVEL (!LCD_BCKL_ON_LEVEL)
#endif
#endif
#ifndef LCD_WIDTH
#ifdef LCD_SWAP_XY
#if LCD_SWAP_XY
#define LCD_WIDTH LCD_VRES
#define LCD_HEIGHT LCD_HRES
#else
#define LCD_WIDTH LCD_HRES
#define LCD_HEIGHT LCD_VRES
#endif
#else
#define LCD_WIDTH LCD_HRES
#define LCD_HEIGHT LCD_VRES
#endif
#endif
#ifndef LCD_BIT_DEPTH
#define LCD_BIT_DEPTH 16
#endif
#ifndef LCD_COLOR_SPACE
#if LCD_BIT_DEPTH <= 8
#define LCD_COLOR_SPACE LCD_COLOR_GSC
#else
#define LCD_COLOR_SPACE LCD_COLOR_RGB
#endif
#endif
#ifndef LCD_X_ALIGN
#define LCD_X_ALIGN 1
#endif
#ifndef LCD_Y_ALIGN
#define LCD_Y_ALIGN 1
#endif
#define LCD_TRANSFER_SIZE (((((LCD_WIDTH*(LCD_HEIGHT+(0!=(LCD_HEIGHT%LCD_DIVISOR)))*LCD_BIT_DEPTH+(LCD_DIVISOR-1))/LCD_DIVISOR))+7)/8)
void lcd_init(void);
void lcd_flush(int x1, int y1, int x2, int y2, void* bitmap);
uint8_t* lcd_buffer1(void);
uint8_t* lcd_buffer2(void);
// define this in your source
void lcd_flush_complete(void);
#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H