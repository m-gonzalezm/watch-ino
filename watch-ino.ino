/****************************************************************************

  Watch-ino
  Smartwatch made with ESP32
  [m-gonzalezm]

  -Specs-
  Testing board: ESP32-C3 SuperMini
  Display: OLED 1.3" 128x64 I2C
  RTC: Mini DS3231 I2C RTC
  Software version: 0.1.3
    + Change testing board
    + Refactor the basecode to new architecture
    + Update display and RTC module
    ~ Change weekdays format
    Dev. beg 21.05.2026
    Dev. end 21.05.2026

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

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RTC_DS3231 rtc;

uint8_t timeout = 10;
uint32_t secondsTimeout;
char hour[9], date[17];
const char * weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char * months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

void draw() {
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 30, hour);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(date)) / 2, 50, date);
}

void setup() {
  pinMode(2, INPUT_PULLUP);
  Wire.begin(5, 6);
  u8g2.begin();
  rtc.begin();
}

void loop() {
  DateTime dateTime = rtc.now();
  sprintf(hour, "%02d:%02d", dateTime.hour(), dateTime.minute());
  sprintf(date, "%s, %s %d", weekdays[dateTime.dayOfTheWeek()], months[dateTime.month() - 1], dateTime.day());
  
  if (!digitalRead(2)) secondsTimeout = dateTime.secondstime() + timeout;

  if (secondsTimeout >= dateTime.secondstime()) {
    u8g2.setPowerSave(0);
    u8g2.clearBuffer();
    draw();
    u8g2.sendBuffer();
  } else u8g2.setPowerSave(1);
}
