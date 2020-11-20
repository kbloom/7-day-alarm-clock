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
#pragma once

#include <avr/pgmspace.h>

const uint8_t kCustomChars[8][8] PROGMEM = {
   {
      0b11111,
      0b11111,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b11111,
      0b11111,
   },
   {
      0b11111,
      0b11111,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
   },
   {
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b10001,
      0b11111,
      0b11111,
   },
   {
      0b11111,
      0b11111,
      0b10000,
      0b10000,
      0b10000,
      0b10000,
      0b11111,
      0b11111,
   },
   {
      0b11111,
      0b11111,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b11111,
      0b11111,
   },
   {
      0b11111,
      0b11111,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
   },
   {
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b01111,
      0b11111,
   },
   {
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
      0b00001,
   },
};

const uint8_t kDigitParts[10][2] = {
   {1,2},
   {7,7},
   {5,3},
   {5,4},
   {2,7},
   {3,6},
   {3,2},
   {5,7},
   {1,0},
   {1,4},
};
   
constexpr int kTop = 0;
constexpr int kBottom = 1;

template <class LCD>
void InstallFont(LCD& lcd) {
   for (int i = 0; i < 8; i++) {
      uint8_t buf[8];
      memcpy_P(buf, kCustomChars[i], 8);
      lcd.createChar(i, buf);
   }
}

template <class LCD>
void WriteDigit(LCD& lcd, uint8_t col, uint8_t digit) {
   lcd.setCursor(col, 0);
   lcd.writeChar(kDigitParts[digit][kTop]);
   lcd.setCursor(col, 1);
   lcd.writeChar(kDigitParts[digit][kBottom]);
}

template <class LCD>
void WriteColon(LCD& lcd, uint8_t col) {
   lcd.setCursor(col, 0);
   lcd.write(0b10100101);
   lcd.setCursor(col, 1);
   lcd.write(0b10100101);
}
