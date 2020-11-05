/*
  Copyright 2020 Google LLC

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#include <stdio.h>
#include <SerLCD.h>
#include <SparkFun_Qwiic_Keypad_Arduino_Library.h>
#include <Wire.h>

KEYPAD keypad;
SerLCD lcd;
FILE* lcd_file;

// Weekdays are numbered 0-6 on the RV1805
const char* kDayNames[] = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat"
};

int WriteToPrint(char c, FILE* f) {
  Print* p = static_cast<Print*>(fdev_get_udata(f));
  p->print(c);
}

FILE* OpenAsFile(Print& p) {
  FILE* f = fdevopen(WriteToPrint, nullptr);
  fdev_set_udata(f, &p);
  return f;
}

char ReadChar() {
  while (1) {
    delay(50);
    keypad.updateFIFO();
    if (keypad.getButton() != 0) {
      return keypad.getButton();
    }
  }
}

enum TimeState {
  ACTIVE,
  INACTIVE,
  SKIPPED,
  INVALID,
};

struct Time {
  uint8_t hours24;
  uint8_t minutes;
  TimeState state = INACTIVE;
  uint8_t hours12();
  const char* amPMString();
};

Time Menu_InputTime();

uint8_t Time::hours12() {
  if (hours24 == 0) {
    return 12;
  }
  if (hours24 > 12) {
    return hours24 - 12;
  }
  return hours24;
}

const char* Time::amPMString() {
  if (hours24 >= 12) {
    return "pm";
  }
  return "am";
}

struct Alarms {
  Time alarms[7];
  bool alarms_on;
};

Alarms alarms;

int Menu_InputWeekday() {
  lcd.clear();
  lcd.println("Enter Weekday");
  lcd.print("1=Sun -- 7=Sat");
  char c = ReadChar();
  if ('1' <= c && c <= '7') {
    return c - '1';
  }
  lcd.clear();
  lcd.println("Invalid time.");
  delay(1000);
  return -1;
}

void Menu_DisplayAlarm(int day) {
  const Time& time = alarms.alarms[day];
  lcd.clear();

  fprintf(lcd_file, "%s %2d:%02d %s\r\n",
          kDayNames[day], time.hours12(), time.minutes, time.amPMString());
  if (time.state == INACTIVE) {
    lcd.print("Off ");
  } else {
    lcd.print("On  ");
  }
}

void Menu_ToggleAlarm(int day, int direction) {
  if (alarms.alarms[day].state == INACTIVE) {
    alarms.alarms[day].state = ACTIVE;
  } else {
    alarms.alarms[day].state = INACTIVE;
  }
}

void Menu_SetClock() {
  int d = Menu_InputWeekday();
  if (d == -1) return;
  Time t = Menu_InputTime();
  if (t.state == INVALID) return;
  lcd.println("Would set clock");
}

void MenuSystem() {
  int cur = -2;
  while (true) {
    lcd.clear();
    if (cur == -2) {
      lcd.println("Would print RTC time");
    }
    if (cur == -1) {
      if (alarms.alarms_on) {
        lcd.println("Alarms Enabled");
      } else {
        lcd.println("Alarms Disabled");
      }
    }
    if (0 <= cur && cur <= 6) {
      Menu_DisplayAlarm(cur);
    }
    if (cur == 7) {
      lcd.println("Volume");
    }
    if (cur == 8) {
      lcd.println("Eq");
    }
    char c = ReadChar();
    if (c == '#' || c == '*') {
      break;
    } else if (c == '2') {
      cur--;
    } else if (c == '8') {
      cur++;
    } else if (0 <= cur && cur <= 6) {
      if (c == '5') {
        Time t = Menu_InputTime();
        if (t.state != INVALID) {
          t.state = alarms.alarms[cur].state;
          alarms.alarms[cur] = t;
        }
      }
      if (c == '4') {
        Menu_ToggleAlarm(cur, -1);
      }
      if (c == '6') {
        Menu_ToggleAlarm(cur, 1);
      }
    } else if (cur == -2 && c == '5') {
      Menu_SetClock();
    } else if (cur == -1 && c == '4' || c == '5' || c == '6') {
      alarms.alarms_on = !alarms.alarms_on;
    }

    if (cur < -2) {
      cur = 8;
    }
    if (cur > 8) {
      cur = -2;
    }
  }
}

Time InputTime() {
  Time t;
  t.state = INVALID;
  lcd.clear();
  lcd.println("Time HH:MM");
  lcd.setCursor(0, 1);
  lcd.println("#=< (24 hours)");
  lcd.blink();
  lcd.setCursor(5, 0);
  char c[3];
  c[2] = 0;

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') goto end;
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') goto end;
  lcd.print(c[1]);
  lcd.print(':');

  t.hours24 = atoi(c);

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') goto end;
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') goto end;
  lcd.print(c[1]);

  t.minutes = atoi(c);

  if (t.hours24 < 24 && t.minutes < 60) {
    t.state = ACTIVE;
  } else {
    lcd.clear();
    lcd.println("Invalid time.");
    delay(1000);
  }

end:
  lcd.noBlink();
  return t;
}

void InputDate() {
  lcd.clear();
  lcd.println("Date YYYY-MM-DD");
  lcd.setCursor(0, 1);
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
  lcd_file = OpenAsFile(lcd);
  keypad.begin();

  lcd.setBacklight(255, 0, 0);
  MenuSystem();
  lcd.clear();
  lcd.println("Done");
}

void loop() {
  // put your main code here, to run repeatedly:

}
