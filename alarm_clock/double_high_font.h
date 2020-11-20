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
#include <Print.h>

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
  {1, 2},
  {7, 7},
  {5, 3},
  {5, 4},
  {2, 7},
  {3, 6},
  {3, 2},
  {5, 7},
  {1, 0},
  {1, 4},
};

constexpr int kTop = 0;
constexpr int kBottom = 1;

template <class LCD>
class DoubleHighFont : public Print {
  private:
    LCD& lcd_;
    uint8_t column_;

  public:
    DoubleHighFont(LCD& lcd): lcd_(lcd) {}

    void Install() {
      for (int i = 0; i < 8; i++) {
        uint8_t buf[8];
        memcpy_P(buf, kCustomChars[i], 8);
        lcd_.createChar(i, buf);
      }
    }

    void SetColumn(uint8_t col) {
      column_ = col;
    }

    size_t write(uint8_t c) override {
      if ('0' <= c && c <= '9') {
        lcd_.setCursor(column_, 0);
        lcd_.writeChar(kDigitParts[c - '0'][kTop]);
        lcd_.setCursor(column_, 1);
        lcd_.writeChar(kDigitParts[c - '0'][kBottom]);
        column_++;
        return 1;
      }
      if (c == ':') {
        lcd_.setCursor(column_, 0);
        lcd_.write(0b10100101);
        lcd_.setCursor(column_, 1);
        lcd_.write(0b10100101);
        column_++;
        return 1;
      }
      if (c == ' ') {
        lcd_.setCursor(column_, 0);
        lcd_.write(' ');
        lcd_.setCursor(column_, 1);
        lcd_.write(' ');
        column_++;
        return 1;
      }
      return 0;
    }
};
