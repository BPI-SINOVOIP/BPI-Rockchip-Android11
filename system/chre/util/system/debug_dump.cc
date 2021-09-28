/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "chre/util/system/debug_dump.h"

#include <cstdio>

#include "chre/platform/log.h"

namespace chre {

void DebugDumpWrapper::print(const char *formatStr, ...) {
  va_list argList;
  va_start(argList, formatStr);
  print(formatStr, argList);
  va_end(argList);
}

void DebugDumpWrapper::print(const char *formatStr, va_list argList) {
  va_list argListCopy;
  va_copy(argListCopy, argList);

  if (mCurrBuff != nullptr || allocNewBuffer()) {
    bool sizeValid;
    size_t sizeOfStr;
    if (!insertString(formatStr, argList, &sizeValid, &sizeOfStr)) {
      if (!sizeValid) {
        LOGE("Error inserting string into buffer in debug dump");
      } else if (sizeOfStr >= kBuffSize) {
        LOGE(
            "String was too large to fit in a single buffer for debug dump "
            "print");
      } else if (allocNewBuffer()) {
        // Insufficient space left in buffer, allocate a new one and it's
        // guaranteed to succeed.
        bool success =
            insertString(formatStr, argListCopy, &sizeValid, &sizeOfStr);
        CHRE_ASSERT(success);
      }
    }
  }
  va_end(argListCopy);
}

bool DebugDumpWrapper::allocNewBuffer() {
  mCurrBuff = static_cast<char *>(memoryAlloc(kBuffSize));
  if (mCurrBuff == nullptr) {
    LOG_OOM();
  } else {
    mBuffers.emplace_back(mCurrBuff);
    mBuffPos = 0;
    mCurrBuff[0] = '\0';
  }
  return mCurrBuff != nullptr;
}

bool DebugDumpWrapper::insertString(const char *formatStr, va_list argList,
                                    bool *sizeValid, size_t *sizeOfStr) {
  CHRE_ASSERT(mCurrBuff != nullptr);
  CHRE_ASSERT(mBuffPos <= kBuffSize);

  // Buffer space left
  size_t spaceLeft = kBuffSize - mBuffPos;

  // Note strLen doesn't count the terminating null character.
  int strLen = vsnprintf(&mCurrBuff[mBuffPos], spaceLeft, formatStr, argList);
  size_t strSize = static_cast<size_t>(strLen);

  bool success = false;
  *sizeValid = false;
  if (strLen >= 0) {
    *sizeValid = true;

    if (strSize >= spaceLeft) {
      // Chop off the incomplete string.
      mCurrBuff[mBuffPos] = '\0';
    } else {
      success = true;
      mBuffPos += strSize;
    }
  }

  *sizeOfStr = strSize;
  return success;
}

}  // namespace chre
