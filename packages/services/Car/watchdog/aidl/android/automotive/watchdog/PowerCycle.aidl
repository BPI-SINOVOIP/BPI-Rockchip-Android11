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
 * Used by ICarWatchdog to describe the device power status.
 */

@VintfStability
@Backing(type="int")
enum PowerCycle {
  /**
   * The system is about to shut down.
   */
  POWER_CYCLE_SHUTDOWN,

  /**
   * The system is being suspended.
   */
  POWER_CYCLE_SUSPEND,

  /**
   * The system resumes working.
   */
  POWER_CYCLE_RESUME,

  /**
   * Number of available power cycles.
   */
  NUM_POWER_CYLES,
}
