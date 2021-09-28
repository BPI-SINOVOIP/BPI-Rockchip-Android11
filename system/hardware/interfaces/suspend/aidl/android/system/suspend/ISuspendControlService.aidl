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

package android.system.suspend;

import android.system.suspend.ISuspendCallback;
import android.system.suspend.WakeLockInfo;

/**
 * Interface exposed by the suspend hal that allows framework to toggle the suspend loop and
 * monitor native wakelocks.
 * @hide
 */
interface ISuspendControlService
{
    /**
     * Starts automatic system suspendion.
     *
     * @return true on success, false otherwise.
     */
    boolean enableAutosuspend();

    /**
     * Registers a callback for suspend events.  ISuspendControlService must keep track of all
     * registered callbacks unless the client process that registered the callback dies.
     *
     * @param callback the callback to register.
     * @return true on success, false otherwise.
     */
    boolean registerCallback(ISuspendCallback callback);

    /**
     * Suspends the system even if there are wakelocks being held.
     */
    boolean forceSuspend();

    /**
     * Returns a list of wake lock stats.
     */
    WakeLockInfo[] getWakeLockStats();
}
