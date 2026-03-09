#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <Datime.h>
#include <FastLED.h>
#include <OpenWeatherMap.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 280

#define TFT_MOSI 4
#define TFT_SCLK 5
#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 15
#define TFT_BL 16

#define WIFI_SSID "qux"
#define WIFI_PSK "changeme"

#define OWM_API_KEY "changeme"

#define OWM_CITY "Philadelphia, PA"
#define OWM_COUNTRY "US"

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET -18000

#define FONT_HEIGHT 16

bool backlight = false;
OpenWeatherMap weather;
Adafruit_ST7789 display(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

static const char* timeStr = "Time: %02d:%02d";
static const char* tempStr = "Temp: %5.1f";
static const char* presStr = "Pres: %5d";
static const char* connectingStr = "Connecting...";
static const char* fetchingStr = "Fetching...";

#define MY_HUE_BLUE 150
#define MY_HUE_RED 10

#define TEMP_MIN 0
#define TEMP_MAX 100

void fetchData() {
  int16_t x, y;
  uint16_t w, h;

  display.getTextBounds(fetchingStr, 0, (SCREEN_WIDTH / 2) - (FONT_HEIGHT / 2),
                        &x, &y, &w, &h);
  x = (SCREEN_HEIGHT / 2) - (w / 2);

  display.setCursor(x, y);
  display.print(fetchingStr);

  OWM_CurrentWeather data;
  weather.getCurrentWeatherByCity(OWM_CITY, OWM_COUNTRY, &data);

  display.fillRect(x, y, w, h, ST77XX_BLACK);
  display.fillRect(0, 0, SCREEN_HEIGHT, SCREEN_WIDTH / 2, ST77XX_BLACK);

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

  display.getTextBounds(thisTime, 12, 10, &x, &y, &w, &h);
  display.setCursor(x, y);
  display.print(thisTime);

  display.getTextBounds(thisTemp, 12, 10 + 26, &x, &y, &w, &h);

  uint8_t hue =
      map8(max((uint8_t)0, (uint8_t)(TEMP_MAX - data.main.temp)) / 255,
           MY_HUE_BLUE, MY_HUE_RED);

  CHSV tempColor = CHSV(hue, 255, 127);
  CRGB tempColorRgb = hsv2rgb_fullspectrum(tempColor);
  uint16_t tempColor565 =
      (tempColorRgb.red << 11) | (tempColorRgb.green << 5) | tempColorRgb.blue;

  display.setTextColor(tempColor565);
  display.setCursor(x, y);
  display.print(thisTemp);

  display.getTextBounds(thisPres, 12, 36 + 26, &x, &y, &w, &h);

  display.setTextColor(ST77XX_WHITE);
  display.setCursor(x, y);
  display.print(thisPres);
}

void toggleBacklight() {
  backlight = !backlight;
  digitalWrite(TFT_BL, backlight ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(50);
  }
  Serial.println("Start running!");

  int16_t x, y;
  uint16_t w, h;

  // enable backlight pin
  pinMode(TFT_BL, OUTPUT);
  toggleBacklight();

  // rotate screen 90 degrees and init all black
  display.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  display.setRotation(1);
  display.fillScreen(ST77XX_BLACK);
  display.setTextSize(3);
  display.setTextColor(ST77XX_WHITE);

  display.getTextBounds(connectingStr, 0,
                        (SCREEN_WIDTH / 2) - (FONT_HEIGHT / 2), &x, &y, &w, &h);
  x = (SCREEN_HEIGHT / 2) - (w / 2);

  display.setCursor(x, y);
  display.print(connectingStr);

  // wait before WiFi connect
  delay(250);

  // connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (!WiFi.isConnected()) {
    delay(1000);
  }

  Serial.println("Connecting!");

  // fill in "Connecting" text
  display.fillRect(x, y, w, h, ST77XX_BLACK);

  // set up OWM API
  weather.begin(OWM_API_KEY);
  weather.setUnits(OWM_UNITS_IMPERIAL);
  weather.setLanguage("en_us");
  weather.setCacheDuration(0);

  configTime(GMT_OFFSET, 0, NTP_SERVER);
}

void loop() {
  fetchData();
  delay(60000);
}
