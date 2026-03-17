#include <esp_http_client.h>
#include <esp_netif_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <htcw_json.h>
#include <roo_time.h>

#include <wifi_manager.hpp>

#include "display.h"
#include "gfx.hpp"
#include "http_stream.hpp"
#include "uix.hpp"

#define TELEGRAMA_IMPLEMENTATION
#include "telegrama.hpp"
#undef TELEGRAMA_IMPLEMENTATION

#define CLOUD_RAIN_ICON_IMPLEMENTATION
#include "cloud_rain_icon.hpp"
#undef CLOUD_RAIN_ICON_IMPLEMENTATION

#define CLOUD_ICON_IMPLEMENTATION
#include "cloud_icon.hpp"
#undef CLOUD_ICON_IMPLEMENTATION

#define CLOUDS_SUN_ICON_IMPLEMENTATION
#include "clouds_sun_icon.hpp"
#undef CLOUDS_SUN_ICON_IMPLEMENTATION

#define SNOWFLAKE_ICON_IMPLEMENTATION
#include "snowflake_icon.hpp"
#undef SNOWFLAKE_ICON_IMPLEMENTATION

#define LIGHTNING_ICON_IMPLEMENTATION
#include "lightning_icon.hpp"
#undef LIGHTNING_ICON_IMPLEMENTATION

#define CLOUD_FOG_ICON_IMPLEMENTATION
#include "cloud_fog_icon.hpp"
#undef CLOUD_FOG_ICON_IMPLEMENTATION

#define CLOUDS_ICON_IMPLEMENTATION
#include "clouds_icon.hpp"
#undef CLOUDS_ICON_IMPLEMENTATION

#define SUN_ICON_IMPLEMENTATION
#include "sun_icon.hpp"
#undef SUN_ICON_IMPLEMENTATION

#define MOON_ICON_IMPLEMENTATION
#include "moon_icon.hpp"
#undef MOON_ICON_IMPLEMENTATION

#define MOON_CLOUD_ICON_IMPLEMENTATION
#include "moon_cloud_icon.hpp"
#undef MOON_CLOUD_ICON_IMPLEMENTATION

#define QUESTION_ICON_IMPLEMENTATION
#include "question_icon.hpp"
#undef QUESTION_ICON_IMPLEMENTATION

#define WIFI_SSID "qux"
#define WIFI_PSK "changeme"

#define OWM_API_URL               \
  "http://api.weatherapi.com/v1/" \
  "current.json?key=changeme&aqi=no&units=imperial&q=Philadelphia%20,PA%20USA"

#define NTP_SERVER "pool.ntp.org"

using namespace gfx;
using namespace uix;
using namespace roo_time;
using namespace esp_idf;
using namespace json;

static const TimeZone TZ(Hours(-4));

static uix::display lcd;
static wifi_manager wifi;
// static OpenWeatherMap weather;

static const char* time_format_string = " %02d:%02d";
static const char* temp_format_string = "%6.1f °F";
static const char* short_temp_format_string = "%.1f°F";
static const char* pres_format_string = "%6d mb";
static const char* hume_format_string = "%6d %%RH";
static const char* connecting_format_string = "Connecting...";
static const char* fetching_format_string = "Fetching...";

// to hold API response data
static char weather_desc[40];
static int sunrise, sunset, pressure, weather_id;
static float temp, humidity;

// to hold strings formatted for display
static char temp_text[15];
static char short_temp_text[15];
static char pres_text[15];
static char time_text[15];
static char hume_text[15];

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

static tt_font small_text_font(telegrama, 10, font_size_units::px);
static tt_font large_text_font(telegrama, 14, font_size_units::px);
static tt_font clock_text_font(telegrama, 28, font_size_units::px);

static screen_t main_screen;
static screen_t loading_screen;

// each control has a "control surface" derived from a type indicated by the
// screen:
using label_t = uix::label<screen_t::control_surface_type>;
using icon_t = uix::image_box<screen_t::control_surface_type>;
using vlabel_t = uix::vlabel<screen_t::control_surface_type>;

label_t loading_label;
label_t time_label;
label_t temp_label;
label_t hume_label;
label_t pres_label;
icon_t weather_icon;

void animate_label(bool state, bool rightAlign = false) {
  auto bounds = srect16(time_label.bounds());
  uint16_t x1 = bounds.x1;

  while (state ? x1-- > (rightAlign ? 16 : 0) : x1++ < LCD_WIDTH - 1) {
    bounds.x1 = x1;
    time_label.bounds(bounds);
    lcd.update();
    vTaskDelay(8335 / portTICK_PERIOD_MS);
  }
}

tm unixToTm(unsigned long timestamp) {
  time_t time = static_cast<time_t>(timestamp);
  return *localtime(&time);
}

void fetch_data() {
  http_handle_t handle = http_init(OWM_API_URL);
  int status = http_read_status_and_headers(handle);

  if (status < 200 || status > 299) {
    // HTTP error
    return;
  }

  enum { J_START = 0, J_CURRENT, J_CURRENT_WEATHER };

  http_stream stream(handle);
  json_reader_ex<64> reader(stream);
  int state = J_START;

  while (reader.read()) {
    if (reader.depth() == 1) {
      if (reader.node_type() == json_node_type::field &&
          !strcmp("current", reader.value())) {
        state = J_CURRENT;
      }
    } else {
      if (state == J_CURRENT) {
        if (reader.node_type() == json_node_type::object &&
            !strcmp("weather", reader.value())) {
          state = J_CURRENT_WEATHER;
        } else if (reader.node_type() != json_node_type::field) {
          continue;
        }

        if (!strcmp("sunrise", reader.value()) && reader.read()) {
          sunrise = reader.value_int();
        } else if (!strcmp("sunset", reader.value()) && reader.read()) {
          sunset = reader.value_int();
        } else if (!strcmp("temp", reader.value()) && reader.read()) {
          temp = reader.value_real();
        } else if (!strcmp("pressure", reader.value()) && reader.read()) {
          pressure = reader.value_int();
        } else if (!strcmp("humidity", reader.value()) && reader.read()) {
          humidity = reader.value_real();
        }
      } else if (state == J_CURRENT_WEATHER) {
        if (reader.node_type() != json_node_type::field) {
          continue;
        }

        if (!strcmp("id", reader.value()) && reader.read()) {
          weather_id = reader.value_int();
        } else if (!strcmp("description", reader.value()) && reader.read()) {
          strcpy(weather_desc, reader.value());
        }
      }
    }
  }

  time_t now;
  time(&now);
  struct tm curTime = unixToTm(now);

  auto timestamp =
      DateTime(curTime.tm_year, curTime.tm_mon, curTime.tm_mday,
               curTime.tm_hour, curTime.tm_min, curTime.tm_sec, 0, TZ);

  uint8_t status_code = floor(weather_id / 100);

  auto sunriseTime = DateTime(unixToTm(sunrise), TZ);
  auto sunsetTime = DateTime(unixToTm(sunset), TZ);
  auto sunriseSeconds = (sunriseTime.hour() * 60 * 60) +
                        (sunriseTime.minute() * 60) + sunriseTime.second();
  auto sunsetSeconds = (sunsetTime.hour() * 60 * 60) +
                       (sunsetTime.minute() * 60) + sunsetTime.second();
  auto timestampSeconds = (timestamp.hour() * 60 * 60) +
                          (timestamp.minute() * 60) + timestamp.second();

  if (status_code == 6) {
    weather_icon.image(snowflake_icon);
  } else if (!strcmp(weather_desc, "few clouds")) {
    if (timestampSeconds >= sunriseSeconds &&
        timestampSeconds <= sunsetSeconds) {
      weather_icon.image(clouds_sun_icon);
    } else {
      weather_icon.image(moon_cloud_icon);
    }
  } else if (!strcmp(weather_desc, "scattered clouds")) {
    weather_icon.image(cloud_icon);
  } else if (!strcmp(weather_desc, "broken clouds") ||
             !strcmp(weather_desc, "overcast clouds")) {
    weather_icon.image(clouds_icon);
  } else if (status_code == 5 || status_code == 3) {
    weather_icon.image(cloud_rain_icon);
  } else if (status_code == 2) {
    weather_icon.image(lightning_icon);
  } else if (status_code == 8) {
    if (timestampSeconds >= sunriseSeconds &&
        timestampSeconds <= sunsetSeconds) {
      weather_icon.image(sun_icon);
    } else {
      weather_icon.image(moon_icon);
    }
  } else if (status_code == 7) {
    weather_icon.image(cloud_fog_icon);
  }

  snprintf(temp_text, sizeof(temp_text), temp_format_string, temp);
  snprintf(pres_text, sizeof(pres_text), pres_format_string, pressure);
  snprintf(time_text, sizeof(time_text), time_format_string, timestamp.hour(),
           timestamp.minute());
  snprintf(hume_text, sizeof(hume_text), hume_format_string, humidity);
  snprintf(short_temp_text, sizeof(short_temp_text), short_temp_format_string,
           temp);

  time_label.text(time_text);
  pres_label.text(pres_text);
  temp_label.text(temp_text);
  hume_label.text(hume_text);

  lcd.update();

  vTaskDelay(5000 / portTICK_PERIOD_MS);
  animate_label(false);
  time_label.text(short_temp_text);
  animate_label(true);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  animate_label(false);
  time_label.text(time_text);
  animate_label(true, true);
}

void app_loop(void* params) {
  uint32_t started = pdTICKS_TO_MS(xTaskGetTickCount());

  fetch_data();

  uint16_t elapsed = pdTICKS_TO_MS(xTaskGetTickCount()) - started;

  vTaskDelay((60000 - elapsed) / portTICK_PERIOD_MS);
}

extern "C" {
void app_main() {
  lcd_init();

  lcd.buffer_size(LCD_TRANSFER_SIZE);
  lcd.buffer1((uint8_t*)lcd_buffer1());
  lcd.buffer2((uint8_t*)lcd_buffer2());
  lcd.on_flush_callback(uix_flush);

  small_text_font.initialize();
  large_text_font.initialize();
  clock_text_font.initialize();

  loading_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});
  loading_screen.background_color(scr_color_t::black);

  loading_label.bounds(srect16(0, 0, LCD_WIDTH, LCD_HEIGHT));
  loading_label.font(large_text_font);
  loading_label.color(uix_color_t::white);
  loading_label.text(connecting_format_string);
  loading_label.text_justify(uix_justify::center);

  loading_screen.register_control(loading_label);

  lcd.active_screen(loading_screen);
  lcd.update();

  wifi.connect(WIFI_SSID, WIFI_PSK);

  loading_label.text(fetching_format_string);
  lcd.update();

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
  esp_netif_sntp_init(&config);

  main_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});
  main_screen.background_color(scr_color_t::black);

  time_label.bounds(srect16(16, LCD_HEIGHT - 32, LCD_WIDTH, LCD_HEIGHT));
  time_label.font(clock_text_font);
  time_label.color(uix_color_t::white);
  time_label.text_justify(uix_justify::top_left);
  main_screen.register_control(time_label);

  temp_label.bounds(srect16(0, 0, LCD_WIDTH - 32, 10));
  temp_label.font(small_text_font);
  temp_label.color(uix_color_t::white);
  temp_label.text_justify(uix_justify::top_left);
  main_screen.register_control(temp_label);

  hume_label.bounds(srect16(0, 10, LCD_WIDTH - 32, 20));
  hume_label.font(small_text_font);
  hume_label.color(uix_color_t::white);
  hume_label.text_justify(uix_justify::top_left);
  main_screen.register_control(hume_label);

  pres_label.bounds(srect16(0, 20, LCD_WIDTH - 32, 30));
  pres_label.font(small_text_font);
  pres_label.color(uix_color_t::white);
  pres_label.text_justify(uix_justify::top_left);
  main_screen.register_control(pres_label);

  weather_icon.bounds(srect16(LCD_WIDTH - 32, 0, LCD_WIDTH, 32));
  weather_icon.image(question_icon);
  main_screen.register_control(weather_icon);

  lcd.active_screen(main_screen);

  fetch_data();

  xTaskCreatePinnedToCore(app_loop, "app", CONFIG_ESP_MAIN_TASK_STACK_SIZE,
                          nullptr, 20, nullptr, 0);
}
}