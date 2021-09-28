/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "common/link_key.h"
#include <gtest/gtest.h>
#include "os/log.h"

using bluetooth::common::LinkKey;

static const char* test_link_key = "4c68384139f574d836bcf34e9dfb01bf\0";

TEST(LinkKeyUnittest, test_constructor_array) {
  uint8_t data[LinkKey::kLength] = {0x4c, 0x87, 0x49, 0xe1, 0x2e, 0x55, 0x0f, 0x7f,
                                    0x60, 0x8b, 0x4f, 0x96, 0xd7, 0xc5, 0xbc, 0x2a};

  LinkKey link_key(data);

  for (int i = 0; i < LinkKey::kLength; i++) {
    ASSERT_EQ(data[i], link_key.link_key[i]);
  }
}

TEST(LinkKeyUnittest, test_from_str) {
  LinkKey link_key;
  LinkKey::FromString(test_link_key, link_key);

  for (int i = 0; i < LinkKey::kLength; i++) {
    ASSERT_EQ(LinkKey::kExample.link_key[i], link_key.link_key[i]);
  }
}

TEST(LinkKeyUnittest, test_to_str) {
  std::string str = LinkKey::kExample.ToString();
  ASSERT_STREQ(str.c_str(), test_link_key);
}