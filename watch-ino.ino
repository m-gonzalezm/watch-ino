/****************************************************************************

  Watch-ino
  Smartwatch made with ESP32
  [m-gonzalezm]

  -Specs-
  Testing board: ESP32-C3 SuperMini
  Display: OLED 1.3" 128x64 I2C
  RTC: Mini DS3231 I2C RTC
  Software version: 0.2.0
    + Implement smart UI navigation system
    + Add menu screen
    - Change power button pin
    + Add select, up/left and down/right buttons
    + Design menu widgets, available on the project folder for editing
    . Minimal logic changes
    Dev. beg 22.05.2026
    Dev. end 24.05.2026

  -Connections-
  Pin       Component
  GPIO 00   Power button
  GPIO 01   Select button
  GPIO 03   Up/Left button
  GPIO 04   Down/Right button
  GPIO 05   OLED SDA, RTC SDA
  GPIO 06   OLED SCL, RTC SCL

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

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RTC_DS3231 rtc;
ButtonConfig powerConfig, selectConfig, upLeftConfig, downRightConfig;
AceButton buttons[] = {
  AceButton(&powerConfig, 0),
  AceButton(&selectConfig, 1),
  AceButton(&upLeftConfig, 3),
  AceButton(&downRightConfig, 4)
};

uint8_t timeout = 10, lastSecond = 0, lastHour = 0, currentOption = 0;
time_t secondsTimeout;
bool forceDisplayRefresh = false;
char hour[9], date[17];
const char * weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char * months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
const char * modules[] = { "Calendar", "Settings", "Exit", };

enum {
  SPLASH,
  HOME,
  MENU
} screen = SPLASH;

const uint8_t STATUS_BITMAPS[][8] PROGMEM = {
  { 0x00, 0x3e, 0x41, 0x1c, 0x22, 0x00, 0x08, 0x00 }, // WiFi
};

const uint8_t MODULE_BITMAPS[][128] PROGMEM = {
  { // Calendar
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x3e, 0xff, 0xfb, 0xfc,
	  0x3e, 0xff, 0xfb, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc,
	  0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc,
	  0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x30, 0x00, 0x00, 0x0c,
	  0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c,
	  0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c,
	  0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c,
	  0x3f, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
  { // Settings
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x1c, 0xc0,
    0x00, 0x00, 0x19, 0x80, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x33, 0x0c, 0x00, 0x00, 0x31, 0x9c,
    0x00, 0x00, 0x30, 0xf4, 0x00, 0x00, 0x30, 0x64, 0x00, 0x00, 0x60, 0x0c, 0x00, 0x00, 0xe0, 0x3c,
    0x00, 0x01, 0xc3, 0xf8, 0x00, 0x03, 0x8f, 0xe0, 0x00, 0x07, 0x1c, 0x00, 0x00, 0x0e, 0x38, 0x00,
    0x00, 0x1c, 0x70, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x71, 0xc0, 0x00, 0x00, 0xe3, 0x80, 0x00,
    0x01, 0xc7, 0x00, 0x00, 0x03, 0x8e, 0x00, 0x00, 0x07, 0x1c, 0x00, 0x00, 0x0e, 0x38, 0x00, 0x00,
    0x1c, 0x70, 0x00, 0x00, 0x18, 0xe0, 0x00, 0x00, 0x31, 0xc0, 0x00, 0x00, 0x33, 0x80, 0x00, 0x00,
    0x1f, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
  { // Exit
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x00, 0x3f, 0xff, 0xfc, 0x00,
    0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00,
    0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x80, 0x30, 0x00, 0x0c, 0xc0,
    0x30, 0x00, 0x0c, 0x60, 0x30, 0x00, 0x00, 0x70, 0x30, 0x00, 0x00, 0x38, 0x30, 0x07, 0xff, 0xfc,
    0x30, 0x07, 0xff, 0xfc, 0x30, 0x00, 0x00, 0x38, 0x30, 0x00, 0x00, 0x70, 0x30, 0x00, 0x0c, 0x60,
    0x30, 0x00, 0x0c, 0xc0, 0x30, 0x00, 0x0c, 0x80, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00,
    0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00, 0x30, 0x00, 0x0c, 0x00,
    0x3f, 0xff, 0xfc, 0x00, 0x3f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
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

void handlePowerEvent(AceButton*, uint8_t event, uint8_t ) {
  time_t now = time(NULL);
  switch (event) {
    case AceButton::kEventPressed:
      if (secondsTimeout >= now) {
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

void handleSelectEvent(AceButton*, uint8_t event, uint8_t ) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventPressed:
        switch (screen) {
          case HOME:
            screen = MENU;
            break;
          case MENU:
            if (currentOption == COUNT_OF(modules) - 1) {
              screen = HOME;
              currentOption = 0;
            }
            break;
        }
        secondsTimeout = now + timeout;
        forceDisplayRefresh = true;
        break;
    }
  }
}

void handleUpLeftEvent(AceButton*, uint8_t event, uint8_t ) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventPressed:
        switch (screen) {
          case HOME:
            break;
          case MENU:
            currentOption = (currentOption + COUNT_OF(modules) - 1) % COUNT_OF(modules);
            break;
        }
        secondsTimeout = now + timeout;
        forceDisplayRefresh = true;
        break;
    }
  }
}

void handleDownRightEvent(AceButton*, uint8_t event, uint8_t ) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventPressed:
        switch (screen) {
          case HOME:
            break;
          case MENU:
            currentOption = (currentOption + 1) % COUNT_OF(modules);
            break;
        }
        secondsTimeout = now + timeout;
        forceDisplayRefresh = true;
        break;
    }
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

void drawMenu() {
  time_t now;
  struct tm internal;
  time(&now);
  localtime_r(&now, &internal);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(modules[currentOption])) / 2, 62, modules[currentOption]);
  sprintf(hour, "%02d:%02d", internal.tm_hour, internal.tm_min);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 10, hour);
  u8g2.drawBitmap(8, 14, 4, 32, MODULE_BITMAPS[(currentOption + COUNT_OF(modules) - 1) % COUNT_OF(modules)]);
  u8g2.drawBitmap(48, 18, 4, 32, MODULE_BITMAPS[currentOption]);
  u8g2.drawBitmap(88, 14, 4, 32, MODULE_BITMAPS[(currentOption + 1) % COUNT_OF(modules)]);
}

void draw() {
  switch (screen) {
    case SPLASH:
      drawSplash();
      break;
    case HOME:
      drawHome();
      break;
    case MENU:
      drawMenu();
      break;
  }
}

void setup() {
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  Wire.begin(5, 6);
  u8g2.begin();
  rtc.begin();
  powerConfig.setEventHandler(handlePowerEvent);
  selectConfig.setEventHandler(handleSelectEvent);
  upLeftConfig.setEventHandler(handleUpLeftEvent);
  downRightConfig.setEventHandler(handleDownRightEvent);
  xTaskCreate(autoSyncTask, "Scheduled sync", 1536, NULL, 1, NULL);
  secondsTimeout = time(NULL) + timeout;
}

void loop() {
  time_t now;
  struct tm internal;
  time(&now);
  localtime_r(&now, &internal);
  for (AceButton& button : buttons) {
    button.check();
  }
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
    if (secondsTimeout >= now) {
      u8g2.setPowerSave(0);
      u8g2.clearBuffer();
      draw();
      u8g2.sendBuffer();
    } else u8g2.setPowerSave(1);
    forceDisplayRefresh = false;
  }
}
