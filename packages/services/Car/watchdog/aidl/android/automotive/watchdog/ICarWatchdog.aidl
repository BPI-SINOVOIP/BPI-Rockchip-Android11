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

import android.automotive.watchdog.ICarWatchdogClient;
import android.automotive.watchdog.ICarWatchdogMonitor;
import android.automotive.watchdog.StateType;
import android.automotive.watchdog.TimeoutLength;

/**
 * ICarWatchdog is an interface implemented by watchdog server.
 * For health check, 4 components are involved: watchdog server, watchdog mediator, watchdog
 * client, and watchdog monitor.
 *   - watchdog server: checks clients' health status by pinging and waiting for the response.
 *   - watchdog mediator: is a watchdog client by reporting its health status to the server, and
 *                        at the same time plays a role of watchdog server by checking its clients'
 *                        health status.
 *   - watchdog client: reports its health status by responding to server's ping within timeout.
 *   - watchdog monitor: captures and reports the process state of watchdog clients.
 */

@VintfStability
interface ICarWatchdog {
  /**
   * Register the client to the watchdog server.
   *
   * @param client              Watchdog client to register.
   * @param timeout             Timeout length specified through enum.
   */
  void registerClient(in ICarWatchdogClient client, in TimeoutLength timeout);

  /**
   * Unregister the client from the watchdog server.
   *
   * @param client              Watchdog client to unregister.
   */
  void unregisterClient(in ICarWatchdogClient client);

  /**
   * Register the mediator to the watchdog server.
   * Note that watchdog mediator is also a watchdog client.
   * The caller should have system UID.
   *
   * @param mediator            Watchdog mediator to register.
   */
  void registerMediator(in ICarWatchdogClient mediator);

  /**
   * Unregister the mediator from the watchdog server.
   * Note that watchdog mediator is also a watchdog client.
   * The caller should have system UID.
   *
   * @param mediator            Watchdog mediator to unregister.
   */
  void unregisterMediator(in ICarWatchdogClient mediator);

  /**
   * Register the monitor to the watchdog server.
   * The caller should have system UID.
   *
   * @param monitor             Watchdog monitor to register.
   */
  void registerMonitor(in ICarWatchdogMonitor monitor);

  /**
   * Unregister the monitor from the watchdog server.
   * The caller should have system UID.
   *
   * @param monitor             Watchdog monitor to unregister.
   */
  void unregisterMonitor(in ICarWatchdogMonitor monitor);

  /**
   * Tell watchdog server that the client is alive.
   *
   * @param client              Watchdog client that is responding.
   * @param sessionId           Session id given by watchdog server.
   */
  void tellClientAlive(in ICarWatchdogClient client, in int sessionId);

  /**
   * Tell watchdog server that the mediator is alive together with the status of clients under
   * the mediator.
   * The caller should have system UID.
   *
   * @param mediator             Watchdog mediator that is responding.
   * @param clientsNotResponding Array of process id of clients which haven't responded to the
   *                             mediator.
   * @param sessionId            Session id given by watchdog server.
   */
  void tellMediatorAlive(
          in ICarWatchdogClient mediator, in int[] clientsNotResponding, in int sessionId);

  /**
   * Tell watchdog server that the monitor has finished dumping process information.
   * The caller should have system UID.
   *
   * @param monitor              Watchdog monitor that is registered to watchdog server.
   * @param pid                  Process id that has been dumped.
   */
  void tellDumpFinished(in ICarWatchdogMonitor monitor, in int pid);

  /**
   * Notify watchdog server about the system state change.
   * The caller should have system UID.
   *
   * @param type                 One of the change types defined in the StateType enum.
   * @param arg1                 First state change information for the specified type.
   * @param arg2                 Second state change information for the specified type.
   *
   * When type is POWER_CYCLE, arg1 should contain the current power cycle of the device.
   * When type is USER_STATE, arg1 and arg2 should contain the user ID and the current user state.
   * When type is BOOT_PHASE, arg1 should contain the current boot phase.
   */
  void notifySystemStateChange(in StateType type, in int arg1, in int arg2);
}
