/****************************************************************************

  Watch-ino
  Smartwatch made with Arduino
  [m-gonzalezm]

  -Specs-
  Testing board: Arduino Uno R3
  Display: OLED 0.96" 128x64

  -Connections-
  Pin   Component
  A4    OLED SDA
  A5    OLED SCL

****************************************************************************/

#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  u8g2.begin();
}

void loop() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_profont11_mr);
    u8g2.drawStr(0, 10, "Hello world");
  } while(u8g2.nextPage());
}
