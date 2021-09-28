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

#include <gtest/gtest.h>
#include <sys/statfs.h>

static constexpr const char kUserdata[] = "/data";

TEST(Userdata, Use4KBlocksOnExt4) {
  struct statfs buf;
  ASSERT_EQ(0, statfs(kUserdata, &buf))
      << "Cannot statfs " << kUserdata << ": " << strerror(errno);
  if (buf.f_type != EXT4_SUPER_MAGIC) {
    GTEST_SKIP() << "Skipping block size requirement check on fs 0x" << std::hex
                 << buf.f_type;
  }
  ASSERT_EQ(4096, buf.f_bsize);
}
