/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdio.h>

#include "disassembler.h"

// Disassembles an APF program. A hex dump of the program is supplied on stdin.
//
// NOTE: This is a simple debugging tool not meant for shipping or production use.  It is by no
//       means hardened against malicious input and contains known vulnerabilities.
//
// Example usage:
// adb shell dumpsys wifi ipmanager | sed '/Last program:/,+1!d;/Last program:/d;s/[ ]*//' | out/host/linux-x86/bin/apf_disassembler
int main(void) {
  uint32_t program_len = 0;
  uint8_t program[10000];

  // Read in hex program bytes
  int byte;
  while (scanf("%2x", &byte) == 1 && program_len < sizeof(program)) {
      program[program_len++] = byte;
  }

  for (uint32_t pc = 0; pc < program_len;) {
      pc = apf_disassemble(program, program_len, pc);
  }
}
