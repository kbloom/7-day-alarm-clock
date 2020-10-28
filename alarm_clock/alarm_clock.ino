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

enum AlarmState {
  WAITING,
  SNOOZING,
  SOUNDING,
};

int WriteToPrint(char c, FILE* f);
FILE* OpenAsFile(Print& p);
bool AlarmTriggeredForTest();
void ExtendSnooze();
void PrintTime();
void TransitionStateTo(AlarmState new_state);


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

AlarmState state;
int snoozeHours = -1;
int snoozeMinutes = -1;
bool snoozePM = false;

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
  rtc.setToCompilerTime();
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
  if (snoozeHours == -1) {
    snoozeHours = rtc.getHours();
    snoozeMinutes = rtc.getMinutes();
    snoozePM = rtc.isPM();
  }
  snoozeMinutes += 1; // TODO: Change to 8 minutes when done testing
  if (snoozeMinutes >= 60) {
    snoozeMinutes -= 60;
    snoozeHours ++;
  }
  if (snoozeHours > 12) {
    snoozeHours -= 12;
    snoozePM = !snoozePM;
  }

  fprintf(serial_file, "Snoozing until %2d:%02d %s\r\n",
          snoozeHours,
          snoozeMinutes,
          snoozePM ? "pm" : "am");
}



void PrintTime() {
  lcd.setCursor(0, 0);
  fprintf(lcd_file, "Now %s %2d:%02d %s",
          kDayNames[rtc.getWeekday()],
          rtc.getHours(),
          rtc.getMinutes(),
          rtc.isPM() ? "pm" : "am");
}


void TransitionStateTo(AlarmState new_state) {
  if (new_state == state) {
    return;
  }
  if (state == SOUNDING) {
    mp3.stop();
  }
  if (state == SNOOZING) {
    snoozeHours = -1;
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
    if (snoozeHours == rtc.getHours() && snoozeMinutes == rtc.getMinutes() && snoozePM == rtc.isPM()) {
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
