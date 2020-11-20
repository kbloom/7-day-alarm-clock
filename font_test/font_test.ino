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

#include <SerLCD.h>
#include "double_high_font.h"

SerLCD lcd;

void setup() {
  Wire.begin();
  lcd.begin(Wire);

  InstallFont(lcd);

  for (int i=0; i < 10; i++) {
    WriteDigit(lcd, i, i);
  }
  WriteColon(lcd, 10);
}

void loop() {
  // put your main code here, to run repeatedly:

}
