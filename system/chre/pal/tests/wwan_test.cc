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

#include "chre/pal/wwan.h"
#include "gtest/gtest.h"

TEST(PalWwanTest, PackUnpackInvalidNrNci) {
  constexpr int64_t kInvalidNci = INT64_MAX;
  struct chreWwanCellIdentityNr nrId;
  chreWwanPackNrNci(kInvalidNci, &nrId);

  EXPECT_EQ(chreWwanUnpackNrNci(&nrId), kInvalidNci);
}

TEST(PalWwanTest, PackUnpackValidNrNci) {
  constexpr int64_t kValidNci = 0xffeedbeef;
  struct chreWwanCellIdentityNr nrId;
  chreWwanPackNrNci(kValidNci, &nrId);

  EXPECT_EQ(chreWwanUnpackNrNci(&nrId), kValidNci);
}

TEST(PalWwanTest, PackUnpackIncorrectNrNci) {
  // A valid NCI is a 36-bit value.
  constexpr int64_t kIncorrectNci = 0x900dbeefdeadbeef;
  struct chreWwanCellIdentityNr nrId;
  chreWwanPackNrNci(kIncorrectNci, &nrId);

  // No bit masking done in chreWwanUnpackNrNci.
  EXPECT_EQ(chreWwanUnpackNrNci(&nrId), kIncorrectNci);
}
