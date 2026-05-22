/****************************************************************************

  Watch-ino
  Smartwatch made with ESP32
  [m-gonzalezm]

  -Specs-
  Testing board: ESP32-C3 SuperMini
  Display: OLED 1.3" 128x64 I2C
  RTC: Mini DS3231 I2C RTC
  Software version: 0.1.6
    ! Fix syncDateTime critical errors
    + Fix initial time values
    + Modularize screen rendering
    + Add provisional splash screen
    Dev. beg 22.05.2026
    Dev. end 22.05.2026

  -Connections-
  Pin       Component
  GPIO 02   Power button
  GPIO 05   OLED SDA
  GPIO 06   OLED SCL

****************************************************************************/

#include <Arduino.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <AceButton.h>
#include "WiFi.h"
#include "credentials.h"

using namespace ace_button;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RTC_DS3231 rtc;
ButtonConfig buttonConfig;
AceButton button(&buttonConfig, 2);

uint8_t timeout = 10, lastSecond = 0, lastHour = 0;
time_t secondsTimeout;
bool forceDisplayRefresh = false;
char hour[9], date[17];
const char * weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char * months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

enum {
  SPLASH,
  HOME,
} screen = SPLASH;

const uint8_t STATUS_BITMAPS[][8] PROGMEM = {
  { 0x00, 0x3e, 0x41, 0x1c, 0x22, 0x00, 0x08, 0x00 },
};

bool isRTCAvailable() {
  Wire.beginTransmission(0x68);
  if (!Wire.endTransmission())
    if (!rtc.lostPower())
      return true;
  return false;
}

void syncDateTime(void *param) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWD);
  uint8_t timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime("CST6", "pool.ntp.org");
    timeout = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && timeout < 10) {
      vTaskDelay(pdMS_TO_TICKS(500));
      timeout++;
    }
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET)
      if (isRTCAvailable())
        rtc.adjust(DateTime(time(NULL)));
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  if (screen == SPLASH) {
    secondsTimeout = time(NULL) + 20 + timeout;
    forceDisplayRefresh = true;
    screen = HOME;
  }
  vTaskDelete(NULL);
}

void autoSyncTask(void *param) {
  xTaskCreate(syncDateTime, "NTP", 4096, NULL, 2, NULL);
  while (true) {
    time_t now;
    struct tm internal;
    time(&now);
    localtime_r(&now, &internal);
    struct tm _internal = internal;
    _internal.tm_hour = 3;
    _internal.tm_min = 0;
    _internal.tm_sec = 0;
    time_t scheduled = mktime(&_internal);
    if (now >= scheduled)
      scheduled += 3600 * 24;
    vTaskDelay(pdMS_TO_TICKS((scheduled - now) * 1000));
    xTaskCreate(syncDateTime, "NTP fetch", 4096, NULL, 2, NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void handleButtonEvent(AceButton*, uint8_t event, uint8_t ) {
  time_t now = time(NULL);
  switch (event) {
    case AceButton::kEventPressed:
        if (secondsTimeout > now) {
          secondsTimeout = now - 1;
          u8g2.setPowerSave(1);
        } else {
          secondsTimeout = now + timeout;
          u8g2.setPowerSave(0);
          forceDisplayRefresh = true;
        }
        break;
  }
}

void drawSplash() {
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth("watch-ino")) / 2, 40, "watch-ino");
}

void drawHome() {
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 30, hour);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(date)) / 2, 50, date);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawXBMP(120, 0, 8, 8, STATUS_BITMAPS[0]);
  }
}

void draw() {
  switch (screen) {
    case SPLASH:
      drawSplash();
      break;
    case HOME:
      drawHome();
      break;
  }
}

void setup() {
  pinMode(2, INPUT_PULLUP);
  Wire.begin(5, 6);
  u8g2.begin();
  rtc.begin();
  buttonConfig.setEventHandler(handleButtonEvent);
  xTaskCreate(autoSyncTask, "Scheduled sync", 1536, NULL, 1, NULL);
  secondsTimeout = time(NULL) + timeout;
}

void loop() {
  time_t now;
  struct tm internal;
  time(&now);
  localtime_r(&now, &internal);
  button.check();
  if (internal.tm_sec != lastSecond || forceDisplayRefresh) {
    if (internal.tm_hour != lastHour) {
      lastHour = internal.tm_hour;
      if (isRTCAvailable()) {
        DateTime dateTime = rtc.now();
        struct timeval updated = { .tv_sec = dateTime.unixtime() };
        settimeofday(&updated, NULL);
      }
    }
    lastSecond = internal.tm_sec;
    sprintf(hour, "%02d:%02d", internal.tm_hour, internal.tm_min);
    sprintf(date, "%s, %s %d", weekdays[internal.tm_wday], months[internal.tm_mon], internal.tm_mday);
    if (secondsTimeout > now) {
      u8g2.setPowerSave(0);
      u8g2.clearBuffer();
      draw();
      u8g2.sendBuffer();
    } else u8g2.setPowerSave(1);
    forceDisplayRefresh = false;
  }
}
