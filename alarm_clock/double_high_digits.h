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

namespace double_high_digits {

const uint8_t kCustomChars[8][8] PROGMEM = {
  {
    0b01110,
    0b11111,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b11111,
    0b01110,
  },
  {
    0b01110,
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
    0b01110,
  },
  {
    0b01111,
    0b11110,
    0b10000,
    0b10000,
    0b10000,
    0b10000,
    0b11111,
    0b11111,
  },
  {
    0b11110,
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
    0b11110,
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

struct CharParts {
  uint8_t top;
  uint8_t bottom;
};

const CharParts kDigitParts[10] PROGMEM = {
  {1, 2},
  {7, 7},
  {5, 3},
  {4, 6},
  {2, 7},
  {3, 6},
  {3, 2},
  {5, 7},
  {0, 2},
  {0, 6},
};

// Templated so that it works with both SerLCD and LiquidCrystal
// (and any other class that implements the createChar method.
template <class LCD>
void Install(LCD& lcd) {
  for (int i = 0; i < 8; i++) {
    uint8_t buf[8];
    memcpy_P(buf, kCustomChars[i], 8);
    lcd.createChar(i, buf);
  }
}

// Templated so that it works with both SerLCD and LiquidCrystal
// (and any other class that implements the same interface).
// The Writer class is separate from the install class, so that you
// can Install directly to an LCD device, and write somewhere else (e.g. a
// buffer class that will later be written to the LCD in one shot.)
template <class LCD, size_t WIDTH = 16>
class Writer : public Print {
  private:
    LCD& lcd_;
    uint8_t column_ = 0;
    uint8_t row_ = 0;

  public:
    Writer(LCD& lcd): lcd_(lcd) {}

    void setCursor(uint8_t col, uint8_t row) {
      column_ = col;
      row_ = row;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
      uint8_t outBuf[WIDTH];
      uint8_t* op = outBuf;
      for (const uint8_t* cp = buffer; cp < buffer + size ; cp++) {
        char c = *cp;
        if ('0' <= c && c <= '9') {
          *op++ = pgm_read_byte(&kDigitParts[c - '0'].top);
        }
        if ( c == ':') {
          *op++ = 0b10100101;
        }
        if (c == ' ') {
          *op++ = ' ';
        }
      }
      lcd_.setCursor(column_, row_);
      lcd_.write(outBuf, op - outBuf);

      op = outBuf;
      for (const uint8_t* cp = buffer; cp < buffer + size ; cp++) {
        char c = *cp;
        if ('0' <= c && c <= '9') {
          *op++ = pgm_read_byte(&kDigitParts[c - '0'].bottom);
        }
        if ( c == ':') {
          *op++ = 0b10100101;
        }
        if (c == ' ') {
          *op++ = ' ';
        }
      }
      lcd_.setCursor(column_, row_ + 1);
      lcd_.write(outBuf, op - outBuf);
      column_ += op - outBuf;
      return op - outBuf;
    }

    size_t write(uint8_t c) override {
      if ('0' <= c && c <= '9') {
        lcd_.setCursor(column_, row_);
        lcd_.writeChar(pgm_read_byte(&kDigitParts[c - '0'].top));
        lcd_.setCursor(column_, row_ + 1);
        lcd_.writeChar(pgm_read_byte(&kDigitParts[c - '0'].bottom));
        column_++;
        return 1;
      }
      if (c == ':') {
        lcd_.setCursor(column_, row_);
        // Why did I identify this character in binary?
        // That's the way it's printed on the HD44780 data sheet.
        lcd_.write(0b10100101);
        lcd_.setCursor(column_, row_ + 1);
        lcd_.write(0b10100101);
        column_++;
        return 1;
      }
      if (c == ' ') {
        lcd_.setCursor(column_, row_);
        lcd_.write(' ');
        lcd_.setCursor(column_, row_ + 1);
        lcd_.write(' ');
        column_++;
        return 1;
      }
      return 0;
    }
};

} // namespace double_high_digits
