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

package android.automotive.watchdog;

/**
 * Used by ICarWatchdog and ICarWatchdogMediator to determine if the clients are in bad state.
 * Watchdog server will decide that the clients are in bad state when they don't respond within
 * the timeout. Different timeouts are used by different clients based on how responsive they
 * should be.
 */

@VintfStability
@Backing(type="int")
enum TimeoutLength {
  /**
   * Timeout is 3 seconds.
   * This is for services which should be responsive.
   */
  TIMEOUT_CRITICAL,

  /**
   * Timeout is 5 seconds.
   * This is for services which are relatively responsive.
   */
  TIMEOUT_MODERATE,

  /**
   * Timeout is 10 seconds.
   * This is for all other services.
   */
  TIMEOUT_NORMAL,
}
