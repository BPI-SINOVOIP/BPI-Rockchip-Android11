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

#ifndef CHRE_PLATFORM_SLPI_PLATFORM_DEBUG_DUMP_MANAGER_BASE_H_
#define CHRE_PLATFORM_SLPI_PLATFORM_DEBUG_DUMP_MANAGER_BASE_H_

#include <cstdbool>
#include <cstddef>
#include <cstdint>

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
#include "ash/debug.h"
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP

namespace chre {

/**
 * SLPI-specific debug dump functionality.
 */
class PlatformDebugDumpManagerBase {
 public:
  /**
   * Constructor that registers to the underlying debug dump utility if
   * available.
   */
  PlatformDebugDumpManagerBase();

  /**
   * Destructor that unregisters to the underlying debug dump utility if
   * available.
   */
  ~PlatformDebugDumpManagerBase();

  /**
   * To be called when receiving a debug dump request from host.
   *
   * @param hostClientId The host client ID that requested the debug dump.
   *
   * @return true if successfully triggered the debug dump process.
   */
  bool onDebugDumpRequested(uint16_t hostClientId);

  /**
   * @see PlatformDebugDumpManager::sendDebugDump
   */
  void sendDebugDumpResult(const char *debugStr, size_t debugStrSize,
                           bool complete);

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  /**
   * Set the ASH debug dump handle.
   */
  void setHandle(uint32_t handle) {
    mHandle = handle;
  }
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP

 protected:
  //! Host client ID that triggered the debug dump process.
  uint16_t mHostClientId = 0;

  //! Number of times sendDebugDumpToHost called with debugStrSize > 0.
  uint32_t mDataCount = 0;

  //! Whenther the last debug dump session has been marked complete.
  bool mComplete = true;

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  //! Upper bound on the largest string size that can be provided in a single
  //! call to sendDebugDump(), including NULL termination.
  static constexpr size_t kDebugDumpStrMaxSize = ASH_DEBUG_DUMP_STR_MAX_SIZE;

  //! ASH debug dump handle.
  uint32_t mHandle = 0;
#else   // CHRE_ENABLE_ASH_DEBUG_DUMP
  static constexpr size_t kDebugDumpStrMaxSize = CHRE_MESSAGE_TO_HOST_MAX_SIZE;
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SLPI_PLATFORM_DEBUG_DUMP_MANAGER_BASE_H_
