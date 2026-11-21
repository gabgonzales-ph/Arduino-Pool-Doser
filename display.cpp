#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "pins.h"
#include "display.h"

#if USE_I2C_LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);
#else
  #include <LiquidCrystal.h>
  static LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
#endif

namespace Display {

static char cache[2][17];

static void fmt(char *dst, const char *src) {
  uint8_t i = 0;
  for (; i < 16 && src[i] != '\0'; i++) dst[i] = src[i];
  for (; i < 16; i++) dst[i] = ' ';
  dst[16] = '\0';
}

void begin() {
#if USE_I2C_LCD
  lcd.init();
  lcd.backlight();
#else
  lcd.begin(16, 2);
#endif
  lcd.clear();
  cache[0][0] = cache[1][0] = '\0';
}

void show(const char *line1, const char *line2) {
  const char *src[2] = {line1, line2};
  for (uint8_t i = 0; i < 2; i++) {
    char buf[17];
    fmt(buf, src[i]);
    if (strcmp(buf, cache[i]) != 0) {   // only redraw changed lines (no flicker)
      lcd.setCursor(0, i);
      lcd.print(buf);
      strcpy(cache[i], buf);
    }
  }
}

}  // namespace Display
