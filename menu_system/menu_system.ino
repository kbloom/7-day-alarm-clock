#include <SerLCD.h>
#include <SparkFun_Qwiic_Keypad_Arduino_Library.h>
#include <Wire.h>

KEYPAD keypad;
SerLCD lcd;

char ReadChar() {
  while(1) {
    delay(50);
    keypad.updateFIFO();
    if (keypad.getButton() != 0) {
      return keypad.getButton();
    }
  }
}

void InputTime() {
  lcd.clear();
  lcd.println("Time HH:MM");
  lcd.setCursor(0,1);
  lcd.println("#=< (24 hours)");
  lcd.blink();
  lcd.setCursor(5, 0);
  char c;
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
  lcd.print(':');
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
 end:
  lcd.noBlink();
}

void InputDate() {
  lcd.clear();
  lcd.println("Date YYYY-MM-DD");
  lcd.setCursor(0,1);
  lcd.println("#=<");
  lcd.blink();
  lcd.setCursor(5, 0);
  char c;
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
  lcd.print('-');
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
  lcd.print('-');
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);  
  if ((c = ReadChar()) == '#') goto end;
  lcd.print(c);
 end:
  lcd.noBlink();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(Wire);
  keypad.begin();

  lcd.setBacklight(255,0,0);
  InputDate();
  InputTime();
}

void loop() {
  // put your main code here, to run repeatedly:

}
