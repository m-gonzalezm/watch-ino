/****************************************************************************

  Watch-ino
  Smartwatch made with Arduino
  [m-gonzalezm]

  -Specs-
  Testing board: Arduino Uno R3
  Display: OLED 0.96" 128x64
  Software version: 0.1.2
    + Implement power button and display timeout
    Dev. beg 29.07.2024
    Dev. end 30.07.2024

  -Connections-
  Pin   Component
  02    Power button
  A4    OLED SDA
  A5    OLED SCL

****************************************************************************/

#include <Arduino.h>
#include <U8g2lib.h>
#include <RTClib.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
RTC_DS1307 rtc;

uint8_t timeout = 10;
uint32_t secondsTimeout;
char hour[9], date[17];
const char * weekdays[] = { "Sun.", "Mon.", "Tues.", "Wed.", "Thurs.", "Fri.", "Sat." };
const char * months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

void draw() {
  u8g2.setFont(u8g2_font_crox5h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(hour)) / 2, 30, hour);
  u8g2.setFont(u8g2_font_crox1h_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(date)) / 2, 50, date);
}

void setup() {
  pinMode(2, INPUT);
  u8g2.begin();
  rtc.begin();
}

void loop() {
  DateTime dateTime = rtc.now();
  sprintf(hour, "%02d:%02d", dateTime.hour(), dateTime.minute());
  sprintf(date, "%s, %s %d", weekdays[dateTime.dayOfTheWeek()], months[dateTime.month() - 1], dateTime.day());
  
  if (digitalRead(2)) secondsTimeout = dateTime.secondstime() + timeout;

  if (secondsTimeout >= dateTime.secondstime()) {
    u8g2.setPowerSave(0);
    u8g2.firstPage();
    do {
      draw();
    } while(u8g2.nextPage());
  } else u8g2.setPowerSave(1);
}
