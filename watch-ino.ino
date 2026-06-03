/****************************************************************************

  Watch-ino
  Smartwatch made with ESP32
  [m-gonzalezm]

  -Specs-
  Testing board: ESP32-C3 SuperMini
  Display: OLED 1.3" 128x64 I2C
  RTC: Mini DS3231 I2C RTC
  Software version: 0.2.1
    + Improvement mayor performance
    + Module task
    * Fix sync flow
    * Fix time managment
    * Optimize clock cycles
    ~ Change leaving to going back
    Dev. beg 24.05.2026
    Dev. end 03.06.2026

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
#include <atomic>
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

TaskHandle_t loopTask = NULL;
uint8_t timeout = 10, currentOption = 0;
time_t lastSecond = 0;
volatile bool forceDisplayRefresh = false;
std::atomic<time_t> secondsTimeout(0);
char hour[9], date[17];
const char * weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char * months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
const char * modules[] = { "Calendar", "Settings", "Back", };

enum {
  SPLASH,
  HOME,
  MENU
} volatile screen = SPLASH;

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
  { // Back
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x00,
    0x3f, 0xff, 0xff, 0x80, 0x1c, 0x00, 0x0f, 0xe0, 0x0e, 0x00, 0x01, 0xf0, 0x06, 0x00, 0x00, 0x70,
    0x03, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x1c,
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x38,
    0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x00, 0x0f, 0xe0,
    0x0f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  }
};

bool isRTCAvailable() {
  Wire.beginTransmission(0x68);
  if (!Wire.endTransmission())
    if (!rtc.lostPower())
      return true;
  return false;
}

bool isTimeAdjusting() {
  struct timeval remaining;
    if (!adjtime(NULL, &remaining))
      return (remaining.tv_sec || remaining.tv_usec);
    return false;
}

void resetTimeout(time_t currentTime) {
  secondsTimeout = currentTime + timeout;
  forceDisplayRefresh = true;
  if (loopTask != NULL)
    xTaskNotifyGive(loopTask);
}

void setTime() {
  if (isTimeAdjusting()) {
    struct timeval zero = { .tv_sec = 0, .tv_usec = 0 };
    adjtime(&zero, NULL);
  }
  DateTime dateTime = rtc.now();
  struct timeval newTime = { .tv_sec = dateTime.unixtime() };
  settimeofday(&newTime, NULL);
}

void adjustTime() {
  DateTime dateTime = rtc.now();
  time_t now = time(NULL);
  struct timeval adjustment = { .tv_sec = dateTime.unixtime() - now };
  adjtime(&adjustment, NULL);
}

bool syncDateTime() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWD);
  uint8_t timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    timeout++;
  }
  bool isSynchronized = false;
  if (WiFi.status() == WL_CONNECTED) {
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    timeout = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && timeout < 10) {
      vTaskDelay(pdMS_TO_TICKS(500));
      timeout++;
    }
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
      isSynchronized = true;
      if (isRTCAvailable())
        rtc.adjust(DateTime(time(NULL)));
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  return isSynchronized;
}

void autoSyncTask(void *param) {
  (void)syncDateTime();
  resetTimeout(time(NULL) + 25LL);
  screen = HOME;
  while (true) {
    time_t now = time(NULL);
    struct tm setDateTime;
    localtime_r(&now, &setDateTime);
    setDateTime.tm_mday++;
    setDateTime.tm_hour = 3;
    setDateTime.tm_min = setDateTime.tm_sec = 0;
    time_t setTime = mktime(&setDateTime);
    vTaskDelay(pdMS_TO_TICKS((setTime - now) * 1000));
    (void)syncDateTime();
  }
}

void autoAdjustTask(void *param) {
  vTaskDelay(pdMS_TO_TICKS(15000));
  while (true) {
    time_t now = time(NULL);
    struct tm targetDateTime;
    localtime_r(&now, &targetDateTime);
    targetDateTime.tm_hour += targetDateTime.tm_min < 15 ? 1 : 2;
    if (targetDateTime.tm_hour == 3)
      targetDateTime.tm_hour++;
    targetDateTime.tm_min = targetDateTime.tm_sec = 0;
    time_t targetTime = mktime(&targetDateTime);
    vTaskDelay(pdMS_TO_TICKS((targetTime - now) * 1000));
    if (isRTCAvailable())
      adjustTime();
  }
}

void buttonPollTask(void *param) {
  TickType_t lastWakeTick = xTaskGetTickCount();
  while (true) {
    vTaskDelayUntil(&lastWakeTick, pdMS_TO_TICKS(20));
    for (AceButton& button : buttons) {
      button.check();
    }
  }
}

void handlePowerEvent(AceButton*, uint8_t event, uint8_t ) {
  if (screen != SPLASH) {
    time_t now = time(NULL);
    switch (event) {
      case AceButton::kEventPressed:
        if (secondsTimeout >= now) {
          secondsTimeout = now - 1;
          u8g2.setPowerSave(1);
        } else {
          resetTimeout(now);
        }
        break;
    }
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
            resetTimeout(now);
            break;
          case MENU:
            if (currentOption == COUNT_OF(modules) - 1) {
              screen = HOME;
              currentOption = 0;
            }
            resetTimeout(now);
            break;
        }
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
            resetTimeout(now);
            break;
        }
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
            resetTimeout(now);
            break;
        }
        break;
    }
  }
}

void drawSplash() {
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth("watch-ino")) / 2, 40, "watch-ino");
}

void drawHome(const tm &dateTime) {
  sprintf(hour, "%02d:%02d", dateTime.tm_hour, dateTime.tm_min);
  sprintf(date, "%s, %s %d", weekdays[dateTime.tm_wday], months[dateTime.tm_mon], dateTime.tm_mday);
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 30, hour);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(date)) / 2, 50, date);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawXBMP(120, 0, 8, 8, STATUS_BITMAPS[0]);
  }
}

void drawMenu(const tm &dateTime) {
  sprintf(hour, "%02d:%02d", dateTime.tm_hour, dateTime.tm_min);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(modules[currentOption])) / 2, 62, modules[currentOption]);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 10, hour);
  u8g2.drawBitmap(8, 14, 4, 32, MODULE_BITMAPS[(currentOption + COUNT_OF(modules) - 1) % COUNT_OF(modules)]);
  u8g2.drawBitmap(48, 18, 4, 32, MODULE_BITMAPS[currentOption]);
  u8g2.drawBitmap(88, 14, 4, 32, MODULE_BITMAPS[(currentOption + 1) % COUNT_OF(modules)]);
}

void draw(const tm &dateTime) {
  switch (screen) {
    case SPLASH:
      drawSplash();
      break;
    case HOME:
      drawHome(dateTime);
      break;
    case MENU:
      drawMenu(dateTime);
      break;
  }
}

void setup() {
  Wire.begin(5, 6);
  u8g2.begin();
  u8g2.clearBuffer();
  drawSplash();
  u8g2.sendBuffer();
  rtc.begin();
  if (isRTCAvailable())
    adjustTime();
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  powerConfig.setEventHandler(handlePowerEvent);
  selectConfig.setEventHandler(handleSelectEvent);
  upLeftConfig.setEventHandler(handleUpLeftEvent);
  downRightConfig.setEventHandler(handleDownRightEvent);
  configTzTime("CST6", "pool.ntp.org");
  loopTask = xTaskGetCurrentTaskHandle();
  xTaskCreate(autoSyncTask, "Scheduled sync", 4096, NULL, 1, NULL);
  xTaskCreate(autoAdjustTask, "Time adjustment", 2048, NULL, 1, NULL);
  xTaskCreate(buttonPollTask, "Button polling", 1536, NULL, 3, NULL);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void loop() {
  time_t now = time(NULL);
  if (now != lastSecond || forceDisplayRefresh) {
    lastSecond = now;
    if (secondsTimeout >= now) {
      struct tm dateTime;
      localtime_r(&now, &dateTime);
      u8g2.setPowerSave(0);
      u8g2.clearBuffer();
      draw(dateTime);
      u8g2.sendBuffer();
      forceDisplayRefresh = false;
    } else {
      u8g2.setPowerSave(1);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }
}
