/****************************************************************************

  Watch-ino
  Smartwatch made with ESP32
  [m-gonzalezm]

  -Specs-
  Testing board: ESP32-C3 SuperMini
  Display: OLED 1.3" 128x64 I2C
  RTC: Mini DS3231 I2C RTC
  Software version: 0.2.2
    + Develop calendar module
    + Implement calendar nav system
      Left click: Previous month
      Right click: Next month
      Double left click: Previous year
      Double right click: Next year
    + Implement shortcuts to return
      Double power click: Home screen
      Double select click: Return to previous screen
    + Design custom font based on crox family
    Dev. beg 03.06.2026
    Dev. end 07.06.2026

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

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

using namespace ace_button;

const uint8_t u8g2_font_crox0hm_tr[684] U8G2_FONT_SECTION("u8g2_font_crox0hm_tr") =
  "\77\0\3\2\3\4\3\5\4\5\12\0\376\7\376\7\376\0t\1\214\2\217 \5\0b\7\60\12<B"
  "\217\22yJ\24\0\61\7\272B\317\322\3\62\14=B\317\222\205\221\224\205\203\0\63\14=B\317\222\205"
  "\221\252%\13\0\64\14=B_&%\245d\320\302\4\65\13=B\307qHC-Y\0\66\14=B"
  "\317\222\211C\222i\311\2\67\13=B\307 f\305,\314\0\70\14=B\317\222i\311\222i\311\2\71"
  "\14=B\317\222i\311\20j\311\2A\14=BW\230%Q\22-\231\26B\15=B\307\220d\332\240"
  "d\332\240\0C\12=B\317\222\211m\311\2D\12=B\307T\311\234\222\11E\13=B\307\61\34\222"
  "\60\34\4F\12=B\307\61\34\222\260\10G\14=B\317\222\211\311\220I\212\22H\12=BGf\33"
  "\206\314\26I\6\71C\307!J\12<B_\233$%\12\0K\15=BG\224\224\64-\211*Y\0"
  "L\10=BG\330\343 M\16=BG\266\14\311\220,\211\222h\1N\13=BG\66MJ\42\335"
  "\2O\12=B\317\222yK\26\0P\14=B\307\220d\332\240\204E\0Q\13E>\317\222\271$\222"
  "\262\6R\14=B\307\220d\332\240d\266\0S\13=B\317\222\251\253\226,\0T\11=B\307 \205"
  "=\1U\11=BG\346[\262\0V\15=BG\246%\245$J\262\60\2W\14=BGfI\224"
  "\344\220l\1X\13=BG\246%\265JM\13Y\12=BG\246%\265\260\11Z\12=B\307 f"
  "\35\7\1a\12-B\317\232\14Z\62\4b\14=BG\30\16If\33\24\0c\12-B\317\222\211"
  "Y\262\0d\12=Bge\320l\311\20e\12-B\317\222\15C:\4f\10\272B\217\262\264\0g"
  "\14=:\317\240\331\222!\34\24\0h\12=BGX\61i\266\0i\7\71CG\62\10j\10I;"
  "G\62\14\1k\15=BG\30%%-\211*Y\0l\6\71C\307!m\13-B\207\322\242$J"
  "\242\24n\11-BGb\322l\1o\11-B\317\222\331\222\5p\14=:\307\220d\266A\11C\0"
  "q\12=:\317\240\331\222!,r\7\252B\307\322\2s\11,B\317\20\212C\2t\11\272BG\222"
  ",\245\0u\11-BG\346\244(\1v\13-BG\246%\245$\213\0w\14-BG\222(\211\322"
  ")I\0x\12,BG$%J$\5y\15=:O\224DI\24ia\244\1z\12,B\307\220"
  "%J\66\4\0\0\0\4\377\377\0";

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
uint8_t timeout = 10;
int16_t currentOption = 0;
time_t lastSecond = 0;
volatile bool forceDisplayRefresh = false;
std::atomic<time_t> secondsTimeout(0);
char hour[9], date[17];
const char *weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char *months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
const char *modules[] = {
  "Calendar",
  "Settings",
  "Back",
};

enum {
  SPLASH,
  HOME,
  MENU,
  CALENDAR
} volatile screen = SPLASH;

const uint8_t STATUS_BITMAPS[][8] PROGMEM = {
  { 0x00, 0x3e, 0x41, 0x1c, 0x22, 0x00, 0x08, 0x00 },  // WiFi
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
    0x3f, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { // Settings
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x00, 0x00, 0x1c, 0xc0,
    0x00, 0x00, 0x19, 0x80, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x33, 0x0c, 0x00, 0x00, 0x31, 0x9c,
    0x00, 0x00, 0x30, 0xf4, 0x00, 0x00, 0x30, 0x64, 0x00, 0x00, 0x60, 0x0c, 0x00, 0x00, 0xe0, 0x3c,
    0x00, 0x01, 0xc3, 0xf8, 0x00, 0x03, 0x8f, 0xe0, 0x00, 0x07, 0x1c, 0x00, 0x00, 0x0e, 0x38, 0x00,
    0x00, 0x1c, 0x70, 0x00, 0x00, 0x38, 0xe0, 0x00, 0x00, 0x71, 0xc0, 0x00, 0x00, 0xe3, 0x80, 0x00,
    0x01, 0xc7, 0x00, 0x00, 0x03, 0x8e, 0x00, 0x00, 0x07, 0x1c, 0x00, 0x00, 0x0e, 0x38, 0x00, 0x00,
    0x1c, 0x70, 0x00, 0x00, 0x18, 0xe0, 0x00, 0x00, 0x31, 0xc0, 0x00, 0x00, 0x33, 0x80, 0x00, 0x00,
    0x1f, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { // Back
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfc, 0x00,
    0x3f, 0xff, 0xff, 0x80, 0x1c, 0x00, 0x0f, 0xe0, 0x0e, 0x00, 0x01, 0xf0, 0x06, 0x00, 0x00, 0x70,
    0x03, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x1c,
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0c,
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x38,
    0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x00, 0x0f, 0xe0,
    0x0f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
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
    vTaskDelayUntil(&lastWakeTick, pdMS_TO_TICKS(10));
    for (AceButton &button : buttons) {
      button.check();
    }
  }
}

void handlePowerEvent(AceButton *, uint8_t event, uint8_t) {
  if (screen != SPLASH) {
    time_t now = time(NULL);
    switch (event) {
      case AceButton::kEventClicked:
        if (secondsTimeout >= now) {
          secondsTimeout = now - 1;
          u8g2.setPowerSave(1);
        } else {
          resetTimeout(now);
        }
        break;
      case AceButton::kEventDoubleClicked:
        screen = HOME;
        currentOption = 0;
        resetTimeout(now);
        break;
    }
  }
}

void handleSelectEvent(AceButton *, uint8_t event, uint8_t) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventClicked:
        switch (screen) {
          case HOME:
            screen = MENU;
            resetTimeout(now);
            break;
          case MENU:
            switch (currentOption) {
              case 0:
                screen = CALENDAR;
                break;
              case COUNT_OF(modules) - 1:
                screen = HOME;
                currentOption = 0;
                break;
            }
            resetTimeout(now);
            break;
        }
        break;
      case AceButton::kEventDoubleClicked:
        switch(screen) {
          case MENU:
            screen = HOME;
            currentOption = 0;
            resetTimeout(now);
            break;
          case CALENDAR:
            screen = MENU;
            currentOption = 0;
            resetTimeout(now);
        }
        break;
    }
  }
}

void handleUpLeftEvent(AceButton *, uint8_t event, uint8_t) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventClicked:
        switch (screen) {
          case HOME:
            break;
          case MENU:
            currentOption = (currentOption + COUNT_OF(modules) - 1) % COUNT_OF(modules);
            resetTimeout(now);
            break;
          case CALENDAR:
            currentOption--;
            resetTimeout(now);
            break;
        }
        break;
      case AceButton::kEventDoubleClicked:
        switch(screen) {
          case CALENDAR:
            currentOption -= 12;
            resetTimeout(now);
        }
        break;
    }
  }
}

void handleDownRightEvent(AceButton *, uint8_t event, uint8_t) {
  time_t now = time(NULL);
  if (secondsTimeout >= now) {
    switch (event) {
      case AceButton::kEventClicked:
        switch (screen) {
          case HOME:
            break;
          case MENU:
            currentOption = (currentOption + 1) % COUNT_OF(modules);
            resetTimeout(now);
            break;
          case CALENDAR:
            currentOption++;
            resetTimeout(now);
            break;
        }
        break;
      case AceButton::kEventDoubleClicked:
        switch(screen) {
          case CALENDAR:
            currentOption += 12;
            resetTimeout(now);
        }
        break;
    }
  }
}

uint8_t getDaysPerMonth(uint8_t month, uint16_t year) {
  static const uint8_t DAYS[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 1 && !(year & 3) && ((year % 100) || !(year % 400)))
    return 29;
  return DAYS[month];
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

void drawCalendar(const tm &dateTime) {
  struct tm calendar = dateTime;
  char weekday[3] = { 0 }, day[3], period[9];
  calendar.tm_mday = 1;
  calendar.tm_mon += currentOption;
  mktime(&calendar);
  sprintf(period, "%.3s %d", months[calendar.tm_mon], calendar.tm_year + 1900);
  u8g2.setFont(u8g2_font_crox0hm_tr);
  for (uint8_t i = 0; i < 7; i++) {
    weekday[0] = weekdays[i][0];
    weekday[1] = weekdays[i][1];
    u8g2.drawStr(i * 18 + 5, 7, weekday);
  }
  for (uint8_t i = 0; i < getDaysPerMonth(calendar.tm_mon, calendar.tm_year + 1900); i++) {
    itoa(i + 1, day, 10);
    u8g2.drawStr((calendar.tm_wday + i) % 7 * 18 + (i < 9) * 3 + 5, (calendar.tm_wday + i) / 7 * 9 + 17, day);
  }
  u8g2.drawStr(77, 62, period);
  if (calendar.tm_year == dateTime.tm_year && calendar.tm_mon == dateTime.tm_mon)
    u8g2.drawFrame(dateTime.tm_wday * 18 + 3, (calendar.tm_wday + dateTime.tm_mday - 1) / 7 * 9 + 8, 15, 11);
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
    case CALENDAR:
      drawCalendar(dateTime);
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
  powerConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  powerConfig.setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  powerConfig.setDoubleClickDelay(250);
  selectConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  selectConfig.setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  selectConfig.setDoubleClickDelay(250);
  upLeftConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  upLeftConfig.setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  upLeftConfig.setDoubleClickDelay(250);
  downRightConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  downRightConfig.setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  downRightConfig.setDoubleClickDelay(250);
  configTzTime("CST6", "pool.ntp.org");
  loopTask = xTaskGetCurrentTaskHandle();
  xTaskCreate(autoSyncTask, "Scheduled sync", 4096, NULL, 1, NULL);
  xTaskCreate(autoAdjustTask, "Time adjustment", 2048, NULL, 1, NULL);
  xTaskCreate(buttonPollTask, "Button polling", 2048, NULL, 3, NULL);
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
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(16));
    } else {
      u8g2.setPowerSave(1);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }
}
