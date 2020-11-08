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
#include <SparkFun_Qwiic_Button.h>
#include <SparkFun_RV1805.h>
#include <SparkFun_Qwiic_MP3_Trigger_Arduino_Library.h>
#include <SparkFun_Qwiic_Keypad_Arduino_Library.h>
#include <SerLCD.h>
#include <stdio.h>

/* I2C addresses:
    0x37: MP3
    0x4B: Keypad
    0x69: RTC
    0x6E: Blue Button
    0x6F: Red Button
    0x72: SerLCD
*/

enum GlobalState {
  WAITING,
  SNOOZING,
  SOUNDING,
};

enum TimeState {
  ACTIVE,
  INACTIVE,
  SKIPPED,
};

struct Time {
  uint8_t hours24;
  uint8_t minutes;
  uint8_t hours12();
  const char* amPMString();
  TimeState state = INACTIVE;
  void AddMinutes(uint8_t minutes);
  static Time FromClock();
  bool operator==(const Time& other) {
    return hours24 == other.hours24 && minutes == other.minutes && state == other.state;
  }
  bool operator<(const Time& other) {
    if (hours24 < other.hours24) {
      return true;
    }
    if (hours24 == other.hours24) {
      return minutes < other.minutes;
    }
    return false;
  }
};


struct Alarms {
  Time alarms[7];
  bool alarms_on;
};

namespace menu {

char ReadChar();
bool InputTime(Time& result);
int InputWeekday();

struct Item {
  virtual void Display() {}
  virtual void Handle(char c) {}
};

struct SetClock : public Item {
  void Display() override;
  void Handle(char c) override;
};

struct AllAlarms : public Item {
  void Display() override;
  void Handle(char c) override;
};

struct SetAlarm : public Item {
    SetAlarm(int day);
    void Display() override;
    void Handle(char c) override;
  private:
    int day_;
};

void Run(const Item** items, int n);

const Item* main[] = {
  new SetClock,
  new AllAlarms,
  new SetAlarm(0),
  new SetAlarm(1),
  new SetAlarm(2),
  new SetAlarm(3),
  new SetAlarm(4),
  new SetAlarm(5),
  new SetAlarm(6),
};

constexpr int kMainLength = sizeof(main) / sizeof(Item*);

} // namespace menu


int WriteToPrint(char c, FILE* f);
FILE* OpenAsFile(Print& p);
bool AlarmTriggeredForTest();
void ExtendSnooze();
void PrintTime();
void TransitionStateTo(GlobalState new_state);

QwiicButton stop_button;
QwiicButton snooze_button;
KEYPAD keypad;
SerLCD lcd;
FILE* lcd_file;
FILE* serial_file;
MP3TRIGGER mp3;
RV1805 rtc;

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

// TODO: change these when I'm done testing
constexpr int kAlarmLength = 1;
constexpr int kSnoozeLength = 1;

GlobalState state;
Time snooze;
Time alarm_stop;
Alarms alarms;


int WriteToPrint(char c, FILE* f) {
  Print* p = static_cast<Print*>(fdev_get_udata(f));
  p->print(c);
}

FILE* OpenAsFile(Print& p) {
  FILE* f = fdevopen(WriteToPrint, nullptr);
  fdev_set_udata(f, &p);
  return f;
}

bool AlarmTriggeredForTest() {
  if (Serial.available()) {
    Serial.read();
    return true;
  }
  return false;
}

void ExtendSnooze() {
  if (snooze.state != ACTIVE) {
    snooze = Time::FromClock();
  }
  snooze.state = ACTIVE;
  snooze.AddMinutes(kSnoozeLength);

  fprintf(serial_file, "Snoozing until %2d:%02d %s\r\n", snooze.hours12(), snooze.minutes, snooze.amPMString());
}

void PrintTime() {
  Time t = Time::FromClock();
  lcd.setCursor(0, 0);
  fprintf(lcd_file, "Now %s %2d:%02d %s",
          kDayNames[rtc.getWeekday()],
          t.hours12(),
          t.minutes,
          t.amPMString());
}

void TransitionStateTo(GlobalState new_state) {
  if (new_state == state) {
    return;
  }
  if (state == SOUNDING) {
    mp3.stop();
    alarm_stop.state = INACTIVE;
  }
  if (state == SNOOZING) {
    snooze.state = INACTIVE;
  }

  Serial.print("Moving to state ");
  state = new_state;

  if (new_state == WAITING) {
    Serial.println("WAITING");
  }
  if (new_state == SOUNDING) {
    Serial.println("SOUNDING");
    mp3.playFile(1);
    alarm_stop = Time::FromClock();
    alarm_stop.AddMinutes(kAlarmLength);
  }
  if (new_state == SNOOZING) {
    Serial.println("SOUNDING");
    ExtendSnooze();
  }
}

Time Time::FromClock() {
  Time t;
  t.hours24 = rtc.getHours();
  t.minutes = rtc.getMinutes();
  t.state = ACTIVE;
  return t;
}

void Time::AddMinutes(uint8_t minutes) {
  this->minutes += minutes;
  if (this->minutes >= 60) {
    uint8_t hours = this->minutes / 60;
    this->minutes %= 60;
    this->hours24 += hours;
    this->hours24 %= 24;
  }
}

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

namespace menu {

char ReadChar() {
  while (1) {
    delay(50);
    keypad.updateFIFO();
    if (keypad.getButton() != 0) {
      return keypad.getButton();
    }
  }
}

int InputWeekday() {
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

bool InputTime(Time& result) {
  Time t;
  lcd.clear();
  lcd.println("Time HH:MM");
  lcd.setCursor(0, 1);
  lcd.println("#=< (24 hours)");
  lcd.blink();
  lcd.setCursor(5, 0);
  char c[3];
  c[2] = 0;

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') goto user_exit;
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') goto user_exit;
  lcd.print(c[1]);
  lcd.print(':');

  t.hours24 = atoi(c);

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') goto user_exit;
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') goto user_exit;
  lcd.print(c[1]);

  t.minutes = atoi(c);
  lcd.noBlink();
  t.state = ACTIVE;

  if (t.hours24 >= 24 || t.minutes >= 60) {
    lcd.clear();
    lcd.println("Invalid time.");
    delay(1000);
    return false;
  }

  result = t;
  return true;

user_exit:
  lcd.noBlink();
  return false;
}

SetAlarm::SetAlarm(int day) : day_(day) {}

void SetAlarm::Display() {
  const Time& time = alarms.alarms[day_];
  lcd.clear();

  fprintf(lcd_file, "%s %2d:%02d %s\r\n",
          kDayNames[day_], time.hours12(), time.minutes, time.amPMString());
  if (time.state == INACTIVE) {
    lcd.print("Off ");
  } else {
    lcd.print("On  ");
  }
}

void SetAlarm::Handle(char c) {
  Time& alarm = alarms.alarms[day_];
  if (c == '5') {
    InputTime(alarm);
  }
  if (c == '4' || c == '6') {
    if (alarm.state == INACTIVE) {
      alarm.state = ACTIVE;
    } else {
      alarm.state = INACTIVE;
    }
  }
}

void SetClock::Display() {
  PrintTime();
  lcd.print("Set Clock");
}

void SetClock::Handle(char c) {
  if (c != '5') return;
  int d = InputWeekday();
  if (d == -1) return;
  Time t;
  if (!InputTime(t)) return;
  rtc.setTime(0, 0, t.minutes, t.hours24, 1, 1, 2000, d);
}

void AllAlarms::Display() {
  if (alarms.alarms_on) {
    lcd.println("Alarms Enabled");
  } else {
    lcd.println("Alarms Disabled");
  }
}

void AllAlarms::Handle(char c) {
  if (c == '4' || c == '5' || c == '6') {
    alarms.alarms_on = !alarms.alarms_on;
  }
}


void Run(const Item** items, const int n) {
  int cur = 0;
  while (true) {
    lcd.clear();
    items[cur]->Display();
    const char c = ReadChar();
    if (c == '#' || c == '*') {
      break;
    }
    if (c == '2' || c == '8') {
      if (c == '2') cur--;
      if (c == '8') cur++;
      if (cur < 0) cur = 0;
      if (cur >= n) cur = n - 1;
      continue;
    }
    items[cur]->Handle(c);
  }
  lcd.clear();
}

} // namespace menu


void setup() {
  bool setup_error = false;
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(Wire);
  if (!stop_button.begin()) {
    Serial.println("Red Button did not acknowledge!");
    setup_error = true;
  }
  if (!snooze_button.begin(0x6E)) {
    Serial.println("Blue Button did not acknowledge!");
    setup_error = true;
  }
  if (!keypad.begin()) {
    Serial.println("Keypad did not acknowledge!");
    // setup_error = true;  // temporarily disabled due to broken keypad
  }
  if (!mp3.begin()) {
    Serial.println("MP3Trigger did not acknowledge!");
    setup_error = true;
  }
  if (!rtc.begin()) {
    Serial.println("RTC did not acknoledge!");
    setup_error = true;
  }
  if (setup_error) {
    Serial.println("Freezing");
    while (1);
  }

  lcd_file = OpenAsFile(lcd);
  serial_file = OpenAsFile(Serial);

  stop_button.setDebounceTime(500);
  snooze_button.setDebounceTime(500);
  rtc.set24Hour();
  lcd.setBacklight(255, 0, 0);
  state = WAITING;
}

void loop() {
  keypad.updateFIFO();
  rtc.updateTime();
  PrintTime();
  if (state == WAITING) {
    if (keypad.getButton() != 0) {
      menu::Run(menu::main, menu::kMainLength);
    } else if (AlarmTriggeredForTest()) {
      TransitionStateTo(SOUNDING);
    } else if (stop_button.hasBeenClicked()) {
      stop_button.clearEventBits();
    } else if (snooze_button.hasBeenClicked()) {
      snooze_button.clearEventBits();
      TransitionStateTo(SNOOZING);
    }
  } else if (state == SNOOZING) {
    if (snooze == Time::FromClock()) {
      TransitionStateTo(SOUNDING);
    } else if (stop_button.hasBeenClicked()) {
      stop_button.clearEventBits();
      TransitionStateTo(WAITING);
    } else if (snooze_button.hasBeenClicked()) {
      snooze_button.clearEventBits();
      ExtendSnooze();
    }
  } else if (state == SOUNDING) {
    // I have a short MP3 that I want to repeat for a few minutes if
    // I'm not around to stop the alarm, so we use an explicit alarm_stop timer,
    // and if the MP3 trigger is found to not be playing, it restarts the alarm.
    // Other arrangements are possible -- for a long MP3, you may not want to loop.
    // In that case you can just use mp3.isPlaying() to determine when to transition back
    // to WAITING, and ignore alarm_stop.
    if (alarm_stop == Time::FromClock()) {
      TransitionStateTo(WAITING);
    } else if (!mp3.isPlaying()) {
      mp3.playFile(1);
    } else if (stop_button.hasBeenClicked()) {
      stop_button.clearEventBits();
      mp3.stop();
      TransitionStateTo(WAITING);
    } else if (snooze_button.hasBeenClicked()) {
      snooze_button.clearEventBits();
      TransitionStateTo(SNOOZING);
    }
  }
  delay(50);
}
