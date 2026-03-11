
#include <Arduino.h>
// display configuration is below:
#include "display.h"

#include "gfx.hpp"
#include "uix.hpp"

#define TELEGRAMA_IMPLEMENTATION
#include "telegrama.hpp"
#undef TELEGRAMA_IMPLEMENTATION

using namespace gfx;
using namespace uix;


static uix::display lcd;

void lcd_flush_complete(void) {
    // let UIX know the DMA transfer completed
    lcd.flush_complete();
}
static void uix_flush(const rect16& bounds, const void* bitmap, void* state) {
    // similar to LVGL
    lcd_flush(bounds.x1,bounds.y1,bounds.x2,bounds.y2,(void*)bitmap);
}
#if LCD_COLOR_SPACE == LCD_COLOR_GSC
#define PIXEL gsc_pixel<LCD_BIT_DEPTH>
#else
#define PIXEL rgb_pixel<LCD_BIT_DEPTH>
#endif
// the following can be abbreviated as
// using screen_t = uix::screen<rgb_pixel<16>>; 
// for most configurations
using screen_t = uix::screen_ex<bitmap<PIXEL>,LCD_X_ALIGN,LCD_Y_ALIGN>;

// native screen color
using scr_color_t = color<screen_t::pixel_type>;
// UIX color (rgba32)
using uix_color_t = color<uix_pixel>;

static tt_font text_font(telegrama,LCD_HEIGHT/8,font_size_units::px);

static screen_t main_screen;

// each control has a "control surface" derived from a type indicated by the screen:
using label_t = uix::label<screen_t::control_surface_type>;

label_t hello;

void setup() {
    lcd_init();
    // setting up the display is similar to LVGL
    lcd.buffer_size(LCD_TRANSFER_SIZE);
    lcd.buffer1((uint8_t*)lcd_buffer1());
    lcd.buffer2((uint8_t*)lcd_buffer2());
    lcd.on_flush_callback(uix_flush);

    text_font.initialize();

    // DO THIS FIRST OR CRASHY CRASHY ON PAINT!
    main_screen.dimensions({LCD_WIDTH,LCD_HEIGHT});

    // the background color of the screen is always
    // in the native screen pixel format. All other
    // UIX business is in UIX pixel format
    main_screen.background_color(scr_color_t::purple);

    // make the label the whole screen
    hello.bounds(main_screen.bounds());
    hello.font(text_font);
    hello.color(uix_color_t::yellow);
    // center the text
    hello.text_justify(uix_justify::center);
    hello.text("hello world!");
    // each control must be registered with the screen it's on
    main_screen.register_control(hello);
    // tell the panel what screen we're on
    lcd.active_screen(main_screen);
}

void loop() {
    lcd.update();
}