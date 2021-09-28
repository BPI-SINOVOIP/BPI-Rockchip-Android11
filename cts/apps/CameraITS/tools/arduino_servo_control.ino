// Copyright 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Import Arduino Servo library
#include <Servo.h>

// Create Servo objects
Servo servo_1;
Servo servo_2;
Servo servo_3;
Servo servo_4;
Servo servo_5;
Servo servo_6;

// Start bits for different items. Note, these much match external script
int servo_start_byte = 255;
int light_start_byte = 254;

// HS-755HB servo datasheet values
int min_pulse_width = 560;   // us
int max_pulse_width = 2500;  // us

// Lighting control channel
int light_channel = 12;  // Arduino D12 pin

// User input for servo and position
int cmd[3];           // raw input from serial buffer, 3 bytes
int servo_num;        // servo number to control
int angle;            // movement angle
int i;                // counter for serial read bytes

void setup() {
  // Attach each Servo object to a digital pin
  servo_1.attach(3, min_pulse_width, max_pulse_width);
  servo_2.attach(5, min_pulse_width, max_pulse_width);
  servo_3.attach(6, min_pulse_width, max_pulse_width);
  servo_4.attach(9, min_pulse_width, max_pulse_width);
  servo_5.attach(10, min_pulse_width, max_pulse_width);
  servo_6.attach(11, min_pulse_width, max_pulse_width);

  // Open the serial connection, 9600 baud
  Serial.begin(9600);

  // Initialize at position 0
  servo_1.write(0);
  servo_2.write(0);
  servo_3.write(0);
  servo_4.write(0);
  servo_5.write(0);
  servo_6.write(0);

  // Create digital output & initialize HIGH
  pinMode(light_channel, OUTPUT);
  digitalWrite(light_channel, HIGH);
}

void loop() {
  if (Serial.available() >= 3) {
    // Read command
    for (i=0; i<3; i++) {
      cmd[i] = Serial.read();
      Serial.write(cmd[i]);
    }
    if (cmd[0] == servo_start_byte) {
      servo_num = cmd[1];
      angle = cmd[2];

      switch (servo_num) {
        case 1:
          servo_1.write(angle);
          break;
        case 2:
          servo_2.write(angle);
          break;
        case 3:
          servo_3.write(angle);
          break;
        case 4:
          servo_4.write(angle);
          break;
        case 5:
          servo_5.write(angle);
          break;
        case 6:
          servo_6.write(angle);
          break;
      }
    }
    else if (cmd[0] == light_start_byte) {
      if (cmd[2] == 0) {
        digitalWrite(light_channel, LOW);
      }
      else if (cmd[2] == 1) {
        digitalWrite(light_channel, HIGH);
      }
    }
  }
}
