#include <SerLCD.h>
#include <Wire.h>

SerLCD lcd;

void setup() {
  Wire.begin();
  lcd.begin(Wire);
}

void loop() {
  char i = 0;
  while (1) {
    lcd.print(i++);
    if (i % 32 == 0) {
      delay(3000);
      lcd.clear();
    }
  }
}
