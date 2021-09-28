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

#ifndef CHRE_CORE_DEBUG_DUMP_MANAGER_H_
#define CHRE_CORE_DEBUG_DUMP_MANAGER_H_

#include <cstdarg>
#include <cstdbool>
#include <cstdint>

#include "chre/core/nanoapp.h"
#include "chre/platform/platform_debug_dump_manager.h"
#include "chre/util/optional.h"
#include "chre/util/system/debug_dump.h"

namespace chre {

/**
 * A helper class that manages the CHRE framework and nanoapp debug dump
 * process.
 */
class DebugDumpManager : public PlatformDebugDumpManager {
 public:
  /**
   * Triggers the CHRE framework and nanoapp debug dump process.
   */
  void trigger();

  /**
   * Helper function to log nanoapp debug dump.
   */
  void appendNanoappLog(const Nanoapp &nanoapp, const char *formatStr,
                        va_list args);

 private:
  //! Utility to hold the framework and nanoapp debug dumps.
  DebugDumpWrapper mDebugDump{kDebugDumpStrMaxSize};

  //! Whether the DebugDumpManager is collecting nanoapp debug dumps.
  bool mCollectingNanoappDebugDumps = false;

  //! Instance ID of the nanoapp that was last logging debug dumps in this
  //! session.
  Optional<uint32_t> mLastNanoappId;

  /**
   * Collect CHRE framework debug dumps.
   */
  void collectFrameworkDebugDumps();

  /**
   * Send collected framework debug dumps to the host.
   */
  void sendFrameworkDebugDumps();

  /**
   * Send collected nanoapp debug dumps to the host.
   */
  void sendNanoappDebugDumps();
};

}  // namespace chre

#endif  // CHRE_CORE_DEBUG_DUMP_MANAGER_H_
