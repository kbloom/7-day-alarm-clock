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
  INVALID,
};

struct Time {
  uint8_t hours;
  uint8_t minutes;
  uint8_t twelveHours();
  const char* amPMString();
  TimeState state = INACTIVE;
  void AddMinutes(uint8_t minutes);
  static Time FromClock();
  bool operator==(const Time& other) {
    return hours == other.hours && minutes == other.minutes && state == other.state;
  }
};

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


const char* kDayNames[] = {
  "",
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
  lcd.setBacklight(255, 0, 0);
  state = WAITING;
}

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
  snooze.AddMinutes(1); // TODO: Change to 8 minutes when done testing

  fprintf(serial_file, "Snoozing until %2d:%02d %s\r\n", snooze.twelveHours(), snooze.minutes, snooze.amPMString());
}



void PrintTime() {
  Time t = Time::FromClock();
  lcd.setCursor(0, 0);
  fprintf(lcd_file, "Now %s %2d:%02d %s",
          kDayNames[rtc.getWeekday()],
          t.twelveHours(),
          t.minutes,
          t.amPMString());
}


void TransitionStateTo(GlobalState new_state) {
  if (new_state == state) {
    return;
  }
  if (state == SOUNDING) {
    mp3.stop();
  }
  if (state == SNOOZING) {
    snooze.state = INACTIVE;
  }
  if (new_state == SOUNDING) {
    mp3.playFile(1);
  }

  state = new_state;
  Serial.print("Moving to state ");
  switch (state) {
    case WAITING:
      Serial.println("WAITING");
      break;
    case SNOOZING:
      Serial.println("SNOOZING");
      ExtendSnooze();
      break;
    case SOUNDING:
      Serial.println("SOUNDING");
      break;
  }
}

Time Time::FromClock() {
  Time t;
  t.hours = rtc.getHours();
  t.minutes = rtc.getMinutes();
  t.state = ACTIVE;
  if (rtc.is12Hour() && rtc.isPM()) {
    t.hours += 12;
  }
  return t;
}

void Time::AddMinutes(uint8_t minutes) {
  this->minutes += minutes;
  if (this->minutes >= 60) {
    uint8_t hours = this->minutes / 60;
    this->minutes %= 60;
    this->hours += hours;
    this->hours %= 24;
  }
}

uint8_t Time::twelveHours() {
  if (hours == 0) {
    return 12;
  }
  if (hours > 12) {
    return hours - 12;
  }
  return hours;
}

const char* Time::amPMString() {
  if (hours >= 12) {
    return "pm";
  }
  return "am";
}

void loop() {
  rtc.updateTime();
  PrintTime();
  if (state == WAITING) {
    if (AlarmTriggeredForTest()) {
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
    if (AlarmTriggeredForTest()) {
      TransitionStateTo(WAITING);
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
