/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <proxy_resolver_v8_wrapper.h>
#include <sys/types.h>
#include <string.h>
#include <codecvt>
#include <fstream>
#include <iostream>
#include "../includes/common.h"

const char16_t* spec = u"";
const char16_t* host = u"";

int main(int argc, char *argv[]) {
  bool shouldRunMultipleTimes = false;
  if (argc != 2) {
    if (argc != 3) {
      std::cout << "incorrect number of arguments" << std::endl;
      std::cout << "usage: ./pacrunner mypac.pac (or)" << std::endl;
      std::cout << "usage: ./pacrunner mypac.pac true" << std::endl;
      return EXIT_FAILURE;
    } else {
      shouldRunMultipleTimes = true;
    }
  }

  ProxyResolverV8Handle* handle = ProxyResolverV8Handle_new();

  std::ifstream t;
  t.open(argv[1]);
  if (t.rdstate() != std::ifstream::goodbit) {
    std::cout << "error opening file" << std::endl;
    return EXIT_FAILURE;
  }
  t.seekg(0, std::ios::end);
  size_t size = t.tellg();
  // allocate an extra byte for the null terminator
  char* raw = (char*)calloc(size + 1, sizeof(char));
  t.seekg(0);
  t.read(raw, size);
  std::string u8Script(raw);
  std::u16string u16Script = std::wstring_convert<
        std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(u8Script);

  ProxyResolverV8Handle_SetPacScript(handle, u16Script.data());
  time_t currentTime = start_timer();
  do {
    ProxyResolverV8Handle_GetProxyForURL(handle, spec, host);
  } while (shouldRunMultipleTimes && timer_active(currentTime));

  ProxyResolverV8Handle_delete(handle);
  return EXIT_SUCCESS;
}
