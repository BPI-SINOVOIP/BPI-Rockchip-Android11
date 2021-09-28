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

#ifndef CHRE_UTIL_SYSTEM_DEBUG_DUMP_H_
#define CHRE_UTIL_SYSTEM_DEBUG_DUMP_H_

#include <cstdarg>
#include <cstddef>

#include <chre/toolchain.h>

#include "chre/util/dynamic_vector.h"
#include "chre/util/unique_ptr.h"

namespace chre {

/**
 * Class to hold information about debug dump buffers so that
 * multiple debug dump commits can be called on buffers.
 */
class DebugDumpWrapper {
 public:
  explicit DebugDumpWrapper(size_t bufferSize)
      : kBuffSize(bufferSize), mCurrBuff(nullptr) {}

  /**
   * Add formatted string to buffers handling allocating a new buffer if
   * necessary.
   *
   * @param formatStr String that should be formatted using the variable arg
   *    list.
   *
   * "this" is the first param in print, so CHRE_PRINTF_ATTR needs to point to
   *    the second and third params.
   */
  CHRE_PRINTF_ATTR(2, 3)
  void print(const char *formatStr, ...);

  /**
   * A version of print that takes arguments as a variable list.
   */
  void print(const char *formatStr, va_list argList);

  /**
   * @return The buffers collected that total up to the full debug dump.
   */
  const DynamicVector<UniquePtr<char>> &getBuffers() const {
    return mBuffers;
  }

  /**
   * Clear all the debug dump buffers.
   */
  void clear() {
    mCurrBuff = nullptr;
    mBuffers.clear();
  }

 private:
  //! Number of bytes allocated for each buffer
  const size_t kBuffSize;
  //! Index that where a string will be inserted into current buffer
  size_t mBuffPos;
  //! Pointer to the current buffer
  char *mCurrBuff;
  //! List of allocated buffers for the debug dump session
  DynamicVector<UniquePtr<char>> mBuffers;

  /**
   * Set the current buffer to new buffer and append it to back of buffers.
   *
   * @return true if successfully allocated memory for new buffer.
   */
  bool allocNewBuffer();

  /**
   * Insert a string onto the end of current buffer.
   *
   * @param formatStr The format string with format specifiers.
   * @param argList The variable list of arguments to be inserted into
   *    formatStr.
   * @param sizeValid The pointer to a variable that will indicate whether
   *    the value stored in sizeOfStr is valid.
   * @param sizeOfStr The pointer to a variable that will be filled with the
   *    size of the string, not including the null terminator.
   *
   * @return true on success.
   */
  bool insertString(const char *formatStr, va_list argList, bool *sizeValid,
                    size_t *sizeOfStr);
};

}  // namespace chre

#endif  // CHRE_UTIL_SYSTEM_DEBUG_DUMP_H_
