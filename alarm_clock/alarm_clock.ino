// vim: sts=2 sw=2 fdm=syntax
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
#include <EEPROM.h>
#include <SparkFun_Qwiic_Button.h>
#include <SparkFun_RV1805.h>
#include <SparkFun_Qwiic_MP3_Trigger_Arduino_Library.h>
#include <SparkFun_Qwiic_Keypad_Arduino_Library.h>
#include <SerLCD.h>
#include <stdio.h>
#include "double_high_digits.h"

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
  SOUNDING_SHABBAT,
};

enum TimeState {
  INACTIVE,
  ACTIVE,
  SKIP_NEXT,
  SHABBAT,
  kMaxTimeState,
};

struct Time {
  uint8_t hours24;
  uint8_t minutes;
  uint8_t hours12() const;
  const char* amPMString() const;
  TimeState state = INACTIVE;
  Time& operator+=(int minutes);
  static Time FromClock();
  bool operator==(const Time& other) const {
    return hours24 == other.hours24 && minutes == other.minutes;
  }
  bool operator<(const Time& other) const {
    if (hours24 < other.hours24) {
      return true;
    }
    if (hours24 == other.hours24) {
      return minutes < other.minutes;
    }
    return false;
  }
};


struct PersistentSettings {
  Time alarms[7];
  bool alarms_off;
};

// Things are organized into namespaces to allow irrelevant sections
// of the code to be folded up when I'm not coding on one of them.
// The use of multiple namespaces was easier than splitting this project
// into several .h and .cc files, because they all have circular dependencies,
// and they all have a dependency on the device objects.
// The namespaces also ensure that related functionality is located
// in close proximity in the code.

namespace menu {

char ReadChar();
bool InputTime(Time& result);
int InputWeekday();

struct Item {
  // Called when the user enters this menu item, or after handling a keypress.
  virtual void Display() {}
  // Handle is used to handle any keypress that's not
  // up/down/exit navigation (which are enforced by
  // menu::Run for menu UI consistency).
  // This function doesn't have to return immediately.
  // It can implement its own UI and read more
  // keypresses itself before returning.
  virtual void Handle(char c) {}
  // Called when the user navigates off of this menu item.
  virtual void Leave() {}
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

struct SoundSettings : public Item {
  void Display() override;
  void Handle(char c) override;
  void Leave() override;
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
  new SoundSettings,
};

constexpr int kMainLength = sizeof(main) / sizeof(Item*);

} // namespace menu

namespace statemachine {

constexpr int kAlarmLength = 5;
constexpr int kSnoozeLength = 8;

void TransitionStateTo(GlobalState new_state);
void ExtendSnooze();
void ToggleSkipped();
void MaybeResetSkipped();
bool AlarmNow();
void Handle();
void HandleForMillis(unsigned long ms);
} // namespace statemachine

namespace display {
void PrintTimeTall();
void PrintNextAlarm();
void PrintShabbatStatus();
void ClearStatusArea();
} // namespace display

int operator-(const Time& t, const Time& u);
int WriteToPrint(char c, FILE* f);
FILE* OpenAsFile(Print& p);
Time& TodaysAlarm();
int NextAlarmDay();

QwiicButton stop_button;
QwiicButton snooze_button;
KEYPAD keypad;
MP3TRIGGER mp3;
RV1805 rtc;

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


GlobalState state;
Time snooze;
Time alarm_stop;
PersistentSettings persistent_settings;

int WriteToPrint(char c, FILE* f) {
  Print* p = static_cast<Print*>(fdev_get_udata(f));
  p->print(c);
}

FILE* OpenAsFile(Print& p) {
  FILE* f = fdevopen(WriteToPrint, nullptr);
  fdev_set_udata(f, &p);
  return f;
}

int NextAlarmDay() {
  if (persistent_settings.alarms_off) return -1;
  int today = rtc.getWeekday();
  if (TodaysAlarm() < Time::FromClock()) {
    today = (today + 1) % 7;
  }
  for (int i = 0; i < 7; i++) {
    int cur_day = (today + i) % 7;
    if (persistent_settings.alarms[cur_day].state != INACTIVE) {
      return cur_day;
    }
  }
  return -1;
}

Time& TodaysAlarm() {
  return persistent_settings.alarms[rtc.getWeekday()];
}

Time Time::FromClock() {
  Time t;
  t.hours24 = rtc.getHours();
  t.minutes = rtc.getMinutes();
  t.state = ACTIVE;
  return t;
}

Time& Time::operator+=(int minutes) {
  this->minutes += minutes;
  if (this->minutes >= 60) {
    uint8_t hours = this->minutes / 60;
    this->minutes %= 60;
    this->hours24 += hours;
    this->hours24 %= 24;
  }
  return *this;
}

int operator-(const Time& t, const Time& u) {
  int diff = (t.hours24 - u.hours24) * 60;
  diff += (t.minutes - u.minutes);
  if (diff < 0) {
    // handle cases of snoozing across midnight correctly
    diff += 24 * 60;
  }
  return diff;
}

uint8_t Time::hours12() const {
  if (hours24 == 0) {
    return 12;
  }
  if (hours24 > 12) {
    return hours24 - 12;
  }
  return hours24;
}

const char* Time::amPMString() const {
  if (hours24 >= 12) {
    return "pm";
  }
  return "am";
}

namespace menu {

// Waits for a keypress on the keypad, and returns the
// keypress. While it waits, it handles statemachine events.
char ReadChar() {
  while (1) {
    delay(50);
    keypad.updateFIFO();
    if (keypad.getButton() != 0) {
      return keypad.getButton();
    }
    statemachine::Handle();
  }
}

int InputWeekday() {
  lcd.clear();
  lcd.println(F("Enter Weekday"));
  lcd.print(F("1=Sun -- 7=Sat"));
  char c = ReadChar();
  if ('1' <= c && c <= '7') {
    return c - '1';
  }
  lcd.clear();
  lcd.println(F("Invalid time."));
  statemachine::HandleForMillis(1000);
  return -1;
}

bool InputTime(Time& result) {
  lcd.clear();
  lcd.println(F("Time HH:MM"));
  lcd.setCursor(0, 1);
  lcd.println(F("#=< (24 hours)"));
  lcd.blink();
  lcd.setCursor(5, 0);
  char c[3];
  c[2] = 0;

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') {
    lcd.noBlink();
    return false;
  }
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') {
    lcd.noBlink();
    return false;
  }
  lcd.print(c[1]);
  lcd.print(':');

  uint8_t hours24 = atoi(c);

  c[0] = ReadChar();
  if (c[0] == '#' || c[0] == '*') {
    lcd.noBlink();
    return false;
  }
  lcd.print(c[0]);

  c[1] = ReadChar();
  if (c[1] == '#' || c[1] == '*') {
    lcd.noBlink();
    return false;
  }
  lcd.print(c[1]);

  uint8_t minutes = atoi(c);
  lcd.noBlink();

  if (hours24 >= 24 || minutes >= 60) {
    lcd.clear();
    lcd.println(F("Invalid time."));
    statemachine::HandleForMillis(1000);
    return false;
  }

  result.hours24 = hours24;
  result.minutes = minutes;
  return true;
}

SetAlarm::SetAlarm(int day) : day_(day) {}

void SetAlarm::Display() {
  const Time& time = persistent_settings.alarms[day_];
  lcd.clear();

  fprintf_P(lcd_file, PSTR("%s %2d:%02d %s\r\n"),
            kDayNames[day_], time.hours12(), time.minutes, time.amPMString());
  switch (time.state) {
    case INACTIVE:
      lcd.print(F("Inactive"));
      break;
    case ACTIVE:
      lcd.print(F("Active"));
      break;
    case SKIP_NEXT:
      lcd.print(F("Skip Next"));
      break;
    case SHABBAT:
      lcd.print(F("Shabbat"));
      break;
  }
}

void SetAlarm::Handle(char c) {
  Time& alarm = persistent_settings.alarms[day_];
  if (c == '5') {
    if (!InputTime(alarm) && alarm.state != SHABBAT) {
      alarm.state = ACTIVE;
    }
  }
  if (c == '4') {
    alarm.state = alarm.state - 1;
    if (alarm.state < 0 || alarm.state >= kMaxTimeState) {
      alarm.state = kMaxTimeState - 1;
    }
  }
  if (c == '6') {
    alarm.state = alarm.state + 1;
    if (alarm.state >= kMaxTimeState) {
      alarm.state = 0;
    }
  }
}

void SetClock::Display() {
  Time t = Time::FromClock();
  lcd.setCursor(0, 0);
  lcd.println(F("Set Clock"));
  fprintf_P(lcd_file, PSTR(" %s %2d:%02d %s"),
            kDayNames[rtc.getWeekday()],
            t.hours12(),
            t.minutes,
            t.amPMString());
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
  if (persistent_settings.alarms_off) {
    lcd.println(F("Alarm Disabled"));
  } else {
    lcd.println(F("Alarm Enabled"));
  }
}

void AllAlarms::Handle(char c) {
  if (c == '4' || c == '5' || c == '6') {
    persistent_settings.alarms_off = !persistent_settings.alarms_off;
  }
}

void SoundSettings::Display() {
  fprintf_P(lcd_file, PSTR("4/6 Volume: %d\r\n"), mp3.getVolume());
  lcd.print("7/9 Eq: ");
  byte eq = mp3.getEQ();
  if (eq == 0) {
    lcd.print(F("Normal"));
  }
  if (eq == 1) {
    lcd.print(F("Pop"));
  }
  if (eq == 2) {
    lcd.print(F("Rock"));
  }
  if (eq == 3) {
    lcd.print(F("Jazz"));
  }
  if (eq == 4) {
    lcd.print(F("Classic"));
  }
  if (eq == 5) {
    lcd.print(F("Bass"));
  }
}

void SoundSettings::Handle(const char c) {
  if (state != SOUNDING && state != SOUNDING_SHABBAT && !mp3.isPlaying()) mp3.playFile(1);
  if (c == '4') {
    mp3.setVolume(mp3.getVolume() - 1);
  }
  if (c == '6') {
    mp3.setVolume(mp3.getVolume() + 1);
  }
  if (c == '7') {
    byte newEq = mp3.getEQ() - 1;
    if (newEq < 0) newEq = 0;
    mp3.setEQ(newEq);
  }
  if (c == '9') {
    byte newEq = mp3.getEQ() + 1;
    if (newEq > 5) newEq = 5;
    mp3.setEQ(newEq);
  }
}

void SoundSettings::Leave() {
  if (state != SOUNDING && state != SOUNDING_SHABBAT) {
    mp3.stop();
  }
}


void Run(const Item** items, const int n) {
  lcd.setFastBacklight(0, 0, 255);
  int cur = 0;
  while (true) {
    lcd.clear();
    items[cur]->Display();
    const char c = ReadChar();
    if (c == '#' || c == '*') {
      items[cur]->Leave();
      break;
    }
    if (c == '2' || c == '8' || c == '0') {
      int old_cur = cur;
      if (c == '2') cur--;
      if (c == '8' || c == '0') cur++;
      if (cur < 0) cur = 0;
      if (cur >= n) cur = n - 1;
      if (old_cur != cur) items[old_cur]->Leave();
      continue;
    }
    items[cur]->Handle(c);
  }
  lcd.clear();
  lcd.setFastBacklight(255, 0, 0);
}

} // namespace menu

namespace statemachine {

void ExtendSnooze() {
  if (snooze.state != ACTIVE) {
    snooze = Time::FromClock();
  }
  snooze.state = ACTIVE;
  snooze += kSnoozeLength;

}

void TransitionStateTo(GlobalState new_state) {
  if (new_state == state) {
    return;
  }
  if (state == SOUNDING_SHABBAT) {
    snooze_button.clearEventBits();
    stop_button.clearEventBits();
  }
  if (state == SOUNDING || state == SOUNDING_SHABBAT) {
    mp3.stop();
    alarm_stop.state = INACTIVE;
  }
  if (state == SNOOZING) {
    snooze.state = INACTIVE;
  }

  state = new_state;

  if (new_state == WAITING) {
  }
  if (new_state == SOUNDING) {
    mp3.playFile(1);
    alarm_stop = Time::FromClock();
    alarm_stop += kAlarmLength;
  }
  if (new_state == SOUNDING_SHABBAT) {
    mp3.playFile(1);
  }
  if (new_state == SNOOZING) {
    ExtendSnooze();
  }
}

void ToggleSkipped() {
  int day = NextAlarmDay();
  if (day == -1) return;
  Time& t = persistent_settings.alarms[day];
  if (t.state == ACTIVE) t.state = SKIP_NEXT;
  else if (t.state == SKIP_NEXT) t.state = ACTIVE;
  EEPROM.put(0, persistent_settings);
}

void MaybeResetSkipped() {
  Time& alarm = TodaysAlarm();
  Time now = Time::FromClock();
  // rtc.getSeconds() == 59 is used to ensure that resetting skipped alarms
  // happens only after they would have triggered, had they not been skipped.
  // Triggering (in AlarmNow) only happens when rtc.getSeconds() == 0.
  if (alarm == now && alarm.state == SKIP_NEXT && rtc.getSeconds() == 59) {
    alarm.state = ACTIVE;
    EEPROM.put(0, persistent_settings);
  }
}

bool AlarmNow() {
  const Time& alarm = TodaysAlarm();
  return !persistent_settings.alarms_off &&
         (alarm.state == ACTIVE || alarm.state == SHABBAT) &&
         alarm == Time::FromClock() &&
         rtc.getSeconds() == 0;
  // If the user stops the alarm within 1
  // minute of it triggering, it is still true that
  // time == Time::FromClock(), and we've returned
  // to the WAITING state, so the alarm will just start
  // sounding again. The check for rtc.getSeconds() == 0
  // prevents that by only allowing us to start the alarm
  // in the first second of the minute. I think it's safe to
  // assume that the user won't react within one second.
}

void Handle() {
  rtc.updateTime();
  Time now = Time::FromClock();
  MaybeResetSkipped();
  if (state == WAITING) {
    bool alarm_now = AlarmNow();
    if (alarm_now && TodaysAlarm().state == ACTIVE) {
      TransitionStateTo(SOUNDING);
    } else if (alarm_now && TodaysAlarm().state == SHABBAT) {
      TransitionStateTo(SOUNDING_SHABBAT);
    }  else if (stop_button.hasBeenClicked()) {
      stop_button.clearEventBits();
      ToggleSkipped();
    } else if (snooze_button.hasBeenClicked()) {
      snooze_button.clearEventBits();
      TransitionStateTo(SNOOZING);
    }
  } else if (state == SNOOZING) {
    if (snooze == now) {
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
    if (alarm_stop == now) {
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
  } else if (state == SOUNDING_SHABBAT) {
    if (TodaysAlarm() == Time::FromClock() && rtc.getSeconds() >= 30) {
      TransitionStateTo(WAITING);
    } else if (!mp3.isPlaying()) {
      mp3.playFile(1);
    }
    // Don't respond to buttons in this mode.
  }
}

// Runs Handle in a loop for ms milliseconds.
// This is used in the menu system instead of delay()
// for long pauses where the menu system expects no user input
// (e.g. when displaying an error message.)
// to ensure that state machine events are handled during that time.
void HandleForMillis(unsigned long ms) {
  unsigned long start = millis();
  do {
    Handle();
    delay(50);
  } while (millis() - start <= ms);
}

} // namespace statemachine

namespace display {

void PrintTimeTall() {
  Time t = Time::FromClock();
  // sprintf is used here (instead of opening font as a FILE* and using fprintf)
  // in order to speed the display by minimizing the number of calls to lcd.setCursor,
  // which is slow. It's not a super big problem, but we do want to call
  // statemachine::Handle at least once a second, and the slow updates were making
  // me nervous.
  char buf[6];
  sprintf_P(buf, PSTR("%2d:%02d"), t.hours12(), t.minutes);
  double_high_digits::Writer<SerLCD> font(lcd);
  font.setCursor(0, 0);
  font.print(buf);
  lcd.setCursor(6, 0);
  lcd.print(kDayNames[rtc.getWeekday()]);
  lcd.setCursor(6, 1);
  lcd.print(t.amPMString());
}

void PrintNextAlarm() {
  int day = NextAlarmDay();
  if (day == -1) {
    ClearStatusArea();
    return;
  }
  lcd.setCursor(13, 0);
  lcd.print(kDayNames[day]);
  lcd.setCursor(12, 1);
  const Time& t = persistent_settings.alarms[day];
  switch (t.state) {
    case INACTIVE:
      lcd.print(F(" Off"));
      break;
    case ACTIVE:
      lcd.print(F("  On"));
      break;
    case SKIP_NEXT:
      lcd.print(F("Skip"));
      break;
    case SHABBAT:
      lcd.print(F("Shbt"));
      break;
  }
}

void PrintShabbatStatus() {
  lcd.setCursor(13, 0);
  lcd.print(kDayNames[rtc.getWeekday()]);
  lcd.setCursor(12, 1);
  lcd.print(F("Shbt"));
}

void ClearStatusArea() {
  lcd.setCursor(13, 0);
  lcd.print(F("   "));
  lcd.setCursor(12, 0);
  lcd.print(F("    "));
}

void PrintMainDisplay() {
  PrintTimeTall();
  if (state == SNOOZING) {
    Time now = Time::FromClock();
    lcd.setCursor(13, 0);
    lcd.print(F("Snz"));
    lcd.setCursor(12, 1);
    fprintf_P(lcd_file, PSTR("%3dm"), snooze - now);
  }
  if (state == SOUNDING_SHABBAT) {
    PrintShabbatStatus();
  }
  if (state == SOUNDING) {
    ClearStatusArea();
  }
  if (state == WAITING) {
    PrintNextAlarm();
  }
}

} // namespace display

void setup() {
  EEPROM.get(0, persistent_settings);
  Wire.begin();
  lcd.begin(Wire);
  stop_button.begin();
  snooze_button.begin(0x6E);
  keypad.begin();
  mp3.begin();
  rtc.begin();

  double_high_digits::Install(lcd);
  lcd_file = OpenAsFile(lcd);

  stop_button.setDebounceTime(1000);
  snooze_button.setDebounceTime(200);
  rtc.set24Hour();
  lcd.setFastBacklight(255, 0, 0);
  state = WAITING;
}

void loop() {
  keypad.updateFIFO();
  if (keypad.getButton() != 0 && state != SOUNDING_SHABBAT) {
    menu::Run(menu::main, menu::kMainLength);
    EEPROM.put(0, persistent_settings);
  }
  statemachine::Handle();
  display::PrintMainDisplay();

  delay(50);
}
