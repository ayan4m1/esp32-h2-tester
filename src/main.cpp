
#include <Arduino.h>
#include <Datime.h>
#include <FastLED.h>
#include <OpenWeatherMap.h>
// display configuration is below:
#include "display.h"
#include "gfx.hpp"
#include "uix.hpp"

#define TELEGRAMA_IMPLEMENTATION
#include "telegrama.hpp"
#undef TELEGRAMA_IMPLEMENTATION

#define CLOUD_RAIN_ICON_IMPLEMENTATION
#include "cloud_rain_icon.hpp"
#undef CLOUD_RAIN_ICON_IMPLEMENTATION

#define CLOUD_SOLID_ICON_IMPLEMENTATION
#include "cloud_solid_icon.hpp"
#undef CLOUD_SOLID_ICON_IMPLEMENTATION

#define CLOUDS_SUN_ICON_IMPLEMENTATION
#include "clouds_sun_icon.hpp"
#undef CLOUDS_SUN_ICON_IMPLEMENTATION

#define SNOWFLAKE_ICON_IMPLEMENTATION
#include "snowflake_icon.hpp"
#undef SNOWFLAKE_ICON_IMPLEMENTATION

#define LIGHTNING_ICON_IMPLEMENTATION
#include "lightning_icon.hpp"
#undef LIGHTNING_ICON_IMPLEMENTATION

#define WIFI_SSID "qux"
#define WIFI_PSK "changeme"

#define OWM_API_KEY "changeme"

#define OWM_CITY "Philadelphia, PA"
#define OWM_COUNTRY "US"

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET -18000

#define TEMP_HUE_COLD 150
#define TEMP_HUE_HOT 10

#define TEMP_COLD 32
#define TEMP_HOT 100

using namespace gfx;
using namespace uix;

static uix::display lcd;
static OpenWeatherMap weather;

static const char* timeStr = "%02d:%02d";
static const char* tempStr = "%5.1f°F";
static const char* presStr = "%5dmb";
static const char* connectingStr = "Connecting...";
static const char* fetchingStr = "Fetching...";

void lcd_flush_complete(void) {
  // let UIX know the DMA transfer completed
  lcd.flush_complete();
}
static void uix_flush(const rect16& bounds, const void* bitmap, void* state) {
  // similar to LVGL
  lcd_flush(bounds.x1, bounds.y1, bounds.x2, bounds.y2, (void*)bitmap);
}
#if LCD_COLOR_SPACE == LCD_COLOR_GSC
#define PIXEL gsc_pixel<LCD_BIT_DEPTH>
#else
#define PIXEL rgb_pixel<LCD_BIT_DEPTH>
#endif
// the following can be abbreviated as
// using screen_t = uix::screen<rgb_pixel<16>>;
// for most configurations
using screen_t = uix::screen_ex<bitmap<PIXEL>, LCD_X_ALIGN, LCD_Y_ALIGN>;

// native screen color
using scr_color_t = color<screen_t::pixel_type>;
// UIX color (rgba32)
using uix_color_t = color<uix_pixel>;

static tt_font text_font(telegrama, LCD_HEIGHT / 8, font_size_units::px);

static screen_t main_screen;

// each control has a "control surface" derived from a type indicated by the
// screen:
using label_t = uix::label<screen_t::control_surface_type>;

using icon_t = uix::image_box<screen_t::control_surface_type>;

label_t timeLabel;
label_t tempLabel;
label_t presLabel;
icon_t weatherIcon;

void fetch_data() {
  OWM_CurrentWeather data;
  weather.getCurrentWeatherByCity(OWM_CITY, OWM_COUNTRY, &data);

  struct tm curTime;
  getLocalTime(&curTime);

  auto timestamp = Datime(curTime.tm_year, curTime.tm_mon, curTime.tm_mday,
                          curTime.tm_hour, curTime.tm_min, curTime.tm_sec);

  char thisTemp[15];
  char thisPres[15];
  char thisTime[15];

  snprintf(thisTemp, sizeof(thisTemp), tempStr, data.main.temp);
  snprintf(thisPres, sizeof(thisPres), presStr, data.main.pressure);
  snprintf(thisTime, sizeof(thisTime), timeStr, timestamp.hour,
           timestamp.minute);

  timeLabel.text(thisTime);
  presLabel.text(thisPres);
  tempLabel.text(thisTemp);

  USBSerial.println(thisTime);
  USBSerial.println(thisPres);
  USBSerial.println(thisTemp);

  timeLabel.invalidate();
  presLabel.invalidate();
  tempLabel.invalidate();

  // float range = TEMP_HOT - TEMP_COLD;
  // float normalized = (data.main.temp - TEMP_COLD) / range;
  // uint8_t hue = map8(normalized * 255, TEMP_HUE_COLD, TEMP_HUE_HOT);
  // CHSV tempColor = CHSV(hue, 255, 127);
  // CRGB tempColorRgb = hsv2rgb_fullspectrum(tempColor);
  // uint16_t tempColor565 =
  //     (tempColorRgb.red << 11) | (tempColorRgb.green << 5) |
  //     tempColorRgb.blue;
}

void setup() {
  USBSerial.begin(115200);
  while (!USBSerial) {
    delay(50);
  }
  USBSerial.println("Starting!");

  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (!WiFi.isConnected()) {
    delay(1000);
  }
  USBSerial.println("Connected!");

  weather.begin(OWM_API_KEY);
  weather.setUnits(OWM_UNITS_IMPERIAL);
  weather.setLanguage("en_us");
  weather.setCacheDuration(0);

  configTime(GMT_OFFSET, 0, NTP_SERVER);

  lcd_init();

  lcd.buffer_size(LCD_TRANSFER_SIZE);
  lcd.buffer1((uint8_t*)lcd_buffer1());
  lcd.buffer2((uint8_t*)lcd_buffer2());
  lcd.on_flush_callback(uix_flush);

  text_font.initialize();
  text_font.size(10, gfx::font_size_units::px);

  // DO THIS FIRST OR CRASHY CRASHY ON PAINT!
  main_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});

  // the background color of the screen is always
  // in the native screen pixel format. All other
  // UIX business is in UIX pixel format
  main_screen.background_color(scr_color_t::black);

  timeLabel.bounds(srect16(0, 0, LCD_WIDTH, 10));
  timeLabel.font(text_font);
  timeLabel.color(uix_color_t::white);
  timeLabel.text_justify(uix_justify::top_left);
  main_screen.register_control(timeLabel);

  tempLabel.bounds(srect16(0, 10, LCD_WIDTH, 20));
  tempLabel.font(text_font);
  tempLabel.color(uix_color_t::white);
  tempLabel.text_justify(uix_justify::top_left);
  main_screen.register_control(tempLabel);

  presLabel.bounds(srect16(0, 20, LCD_WIDTH, 30));
  presLabel.font(text_font);
  presLabel.color(uix_color_t::white);
  presLabel.text_justify(uix_justify::top_left);
  main_screen.register_control(presLabel);

  weatherIcon.bounds(
      srect16(LCD_WIDTH - 32, LCD_HEIGHT - 32, LCD_WIDTH, LCD_HEIGHT));
  weatherIcon.image(lightning_icon);
  main_screen.register_control(weatherIcon);

  lcd.active_screen(main_screen);
}

void loop() {
  lcd.update();
  EVERY_N_MINUTES(1) { fetch_data(); }
}