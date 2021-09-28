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

package android.car.testapi;

import android.os.Looper;

/** Used to manipulate properties of the CarAppFocusManager in unit tests. */
public interface CarAppFocusController {
    /** Set what the UID of the "foreground" app is */
    void setForegroundUid(int uid);

    /** Set what the PID of the "foreground" app is */
    void setForegroundPid(int pid);

    /** Resets the foreground UID such that all UIDs are considered to be foreground */
    void resetForegroundUid();

    /** Resets the foreground PID such that all PIDs are considered to be foreground */
    void resetForegroundPid();

    /** Gets the {@link android.os.Looper} for the internal handler for the service */
    Looper getLooper();
}
