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

#include "gtest/gtest.h"

#include <stdint.h>

#include "chre/util/system/debug_dump.h"

using chre::DebugDumpWrapper;

static constexpr size_t kStandardBufferSize = 4000;

TEST(DebugDumpWrapper, ZeroBuffersInitially) {
  DebugDumpWrapper debugDump(kStandardBufferSize);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_TRUE(buffers.empty());
}

TEST(DebugDumpWrapper, OneBufferForOneString) {
  DebugDumpWrapper debugDump(kStandardBufferSize);
  const char *str = "Lorem ipsum";
  debugDump.print("%s", str);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 1);
  EXPECT_TRUE(strcmp(buffers.front().get(), str) == 0);
}

TEST(DebugDumpWrapper, TwoStringsFitPerfectlyInOneBuffer) {
  DebugDumpWrapper debugDump(5);
  const char *str1 = "ab";
  const char *str2 = "cd";
  debugDump.print("%s", str1);
  debugDump.print("%s", str2);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 1);
  char bothStr[5];
  strcpy(bothStr, str1);
  strcat(bothStr, str2);
  EXPECT_TRUE(strcmp(buffers.front().get(), bothStr) == 0);
}

TEST(DebugDumpWrapper, TooLargeOfStringToFit) {
  DebugDumpWrapper debugDump(1);
  const char *str = "a";
  debugDump.print("%s", str);
  const auto &buffers = debugDump.getBuffers();

  // One null-terminated buffer will be created for an empty wrapper.
  EXPECT_EQ(buffers.size(), 1);
  EXPECT_TRUE(strcmp(buffers.back().get(), "") == 0);

  // Once there's a buffer, it won't be updated.
  debugDump.print("%s", str);
  EXPECT_EQ(buffers.size(), 1);
  EXPECT_TRUE(strcmp(buffers.back().get(), "") == 0);
}

TEST(DebugDumpWrapper, TooLargeOfStringWithPartlyFilledBuffer) {
  DebugDumpWrapper debugDump(2);
  const char *str1 = "a";
  const char *str2 = "bc";
  debugDump.print("%s", str1);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 1);
  debugDump.print("%s", str2);
  EXPECT_EQ(buffers.size(), 1);
  EXPECT_TRUE(strcmp(buffers.front().get(), str1) == 0);
}

TEST(DebugDumpWrapper, StringForcesNewBufferWithPartlyFilledBuffer) {
  DebugDumpWrapper debugDump(4);
  const char *str1 = "ab";
  const char *str2 = "bc";
  debugDump.print("%s", str1);
  debugDump.print("%s", str2);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 2);
  EXPECT_TRUE(strcmp(buffers.front().get(), str1) == 0);
  EXPECT_TRUE(strcmp(buffers.back().get(), str2) == 0);
}

TEST(DebugDumpWrapper, ManyNewBuffersAllocated) {
  DebugDumpWrapper debugDump(kStandardBufferSize);
  constexpr size_t kSizeStrings = 10;
  constexpr size_t kNumPrints = 1200;
  // Should be about 12000 chars added to debugDump
  char str[kSizeStrings];
  memset(str, 'a', sizeof(char) * kSizeStrings);
  str[kSizeStrings - 1] = '\0';
  for (size_t i = 0; i < kNumPrints; i++) {
    debugDump.print("%s", str);
  }
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 3);
}

TEST(DebugDumpWrapper, EmptyStringAllocsOneBuffer) {
  DebugDumpWrapper debugDump(kStandardBufferSize);
  debugDump.print("%s", "");
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 1);
}

TEST(DebugDumpWrapper, BuffersClear) {
  DebugDumpWrapper debugDump(4);
  const char *str1 = "ab";
  const char *str2 = "cd";
  const char *str3 = "ef";

  debugDump.print("%s", str1);
  debugDump.print("%s", str2);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 2);
  EXPECT_TRUE(strcmp(buffers.front().get(), str1) == 0);
  EXPECT_TRUE(strcmp(buffers.back().get(), str2) == 0);

  debugDump.clear();
  EXPECT_EQ(buffers.size(), 0);

  debugDump.print("%s", str3);
  EXPECT_EQ(buffers.size(), 1);
  EXPECT_TRUE(strcmp(buffers.front().get(), str3) == 0);
}

void printVaList(DebugDumpWrapper *debugDump, const char *formatStr, ...) {
  va_list args;
  va_start(args, formatStr);
  debugDump->print(formatStr, args);
  va_end(args);
}

TEST(DebugDumpWrapper, PrintVaListTwoStrings) {
  DebugDumpWrapper debugDump(5);
  const char *str1 = "ab";
  const char *str2 = "cd";
  printVaList(&debugDump, "%s", str1);
  printVaList(&debugDump, "%s", str2);
  const auto &buffers = debugDump.getBuffers();
  EXPECT_EQ(buffers.size(), 1);
  char bothStr[5];
  strcpy(bothStr, str1);
  strcat(bothStr, str2);
  EXPECT_TRUE(strcmp(buffers.front().get(), bothStr) == 0);
}
