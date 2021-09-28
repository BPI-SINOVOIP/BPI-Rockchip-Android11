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

import android.automotive.watchdog.TimeoutLength;

@VintfStability
oneway interface ICarWatchdogClient {
  /**
   * Check if the client is alive.
   * Watchdog server or mediator calls this method, expecting the clients will respond within
   * timeout. The final timeout is decided by the server, considering the requested timeout on
   * client registration. If no response from the clients, watchdog server will dump process
   * information and kill them.
   *
   * @param sessionId           Unique id to identify each health check session.
   * @param timeout             Final timeout given by the server based on client request.
   */
  void checkIfAlive(in int sessionId, in TimeoutLength timeout);

  /**
   * Notify the client that it will be forcedly terminated in 1 second.
   */
  void prepareProcessTermination();
}
