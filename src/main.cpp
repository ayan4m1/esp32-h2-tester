#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>
#include <Datime.h>
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

OpenWeatherMap weather;
Adafruit_ST7789 display(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

static const char* timeStr = "Time: %02d:%02d";
static const char* tempStr = "Temp: %.1f";
static const char* presStr = "Pres: %d";
static const char* connectingStr = "Connecting...";
static const char* fetchingStr = "Fetching...";

void fetchData() {
  int16_t x, y;
  uint16_t w, h;

  display.getTextBounds(fetchingStr, 0, (SCREEN_HEIGHT / 2) - (FONT_HEIGHT / 2),
                        &x, &y, &w, &h);
  x = (SCREEN_WIDTH / 2) - (w / 2);

  display.setCursor(x, y);
  display.print(fetchingStr);

  OWM_CurrentWeather data;
  weather.getCurrentWeatherByCity(OWM_CITY, OWM_COUNTRY, &data);

  display.fillRect(x, y, w, h, ST77XX_BLACK);
  display.fillRect(0, 0, SCREEN_WIDTH, 60, ST77XX_BLACK);

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

  display.getTextBounds(thisTemp, 12, 10 + 18, &x, &y, &w, &h);

  display.setCursor(x, y);
  display.print(thisTemp);

  display.getTextBounds(thisPres, 12, 28 + 18, &x, &y, &w, &h);

  display.setCursor(x, y);
  display.print(thisPres);
}

void setup() {
  int16_t x, y;
  uint16_t w, h;

  // enable backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // flip screen 180 degrees and init all black
  display.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  display.setRotation(2);
  display.fillScreen(ST77XX_BLACK);
  display.setTextSize(2);
  display.setTextColor(ST77XX_WHITE);

  display.getTextBounds(connectingStr, 0,
                        (SCREEN_HEIGHT / 2) - (FONT_HEIGHT / 2), &x, &y, &w,
                        &h);
  x = (SCREEN_WIDTH / 2) - (w / 2);

  display.setCursor(x, y);
  display.print(connectingStr);

  // wait before WiFi connect
  delay(250);

  // connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (!WiFi.isConnected()) {
    delay(1000);
  }

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
