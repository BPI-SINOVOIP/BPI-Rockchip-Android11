/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_PLATFORM_DEBUG_DUMP_MANAGER_H_
#define CHRE_PLATFORM_PLATFORM_DEBUG_DUMP_MANAGER_H_

#include <cstdbool>
#include <cstddef>

#include "chre/target_platform/platform_debug_dump_manager_base.h"
#include "chre/util/non_copyable.h"
#include "chre_api/chre/re.h"

namespace chre {

/**
 * The common interface to debug dump functionality that has platform-specific
 * implementation but must be supported for every platform.
 */
class PlatformDebugDumpManager : public PlatformDebugDumpManagerBase,
                                 public NonCopyable {
 public:
  /**
   * Adds an ASCII string to appear in the debug dump and sends it to the host.
   *
   * Strings longer than kDebugDumpStrMaxSize will be truncated.
   *
   * @param debugStr A null-terminated string containing debug data. Must be a
   *        valid pointer, but can be an empty string.
   * @param complete true if no more debug data is expected.
   */
  void sendDebugDump(const char *debugStr, bool complete);

 private:
  //! kDebugDumpStrMaxSize must be provided by PlatformDebugDumpManagerBase.
  //!
  //! It sets an upper bound on the largest string size that can be provided in
  //! a single call to sendDebugDump(), including null termination, without
  //! getting truncated.
  static_assert(kDebugDumpStrMaxSize >= CHRE_DEBUG_DUMP_MINIMUM_MAX_SIZE,
                "kDebugDumpStrMaxSize must be >="
                " CHRE_DEBUG_DUMP_MINIMUM_MAX_SIZE");
};

}  // namespace chre

#endif  // CHRE_PLATFORM_PLATFORM_DEBUG_DUMP_MANAGER_H_
