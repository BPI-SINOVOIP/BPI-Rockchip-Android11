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


/**
 * Parcelable WakelockInfo - Representation of wake lock stats.
 *
 * Wakelocks obtained via SystemSuspend are hereafter referred to as
 * native wake locks.
 *
 * @name:               Name of wake lock (Not guaranteed to be unique).
 * @activeCount:        Number of times the wake lock was activated.
 * @lastChange:         Monotonic time (in ms) when the wake lock was last touched.
 * @maxTime:            Maximum time (in ms) this wake lock has been continuously active.
 * @totalTime:          Total time (in ms) this wake lock has been active.
 * @isActive:           Status of wake lock.
 * @activeTime:         Time since wake lock was activated, 0 if wake lock is not active.
 * @isKernelWakelock:   True if kernel wake lock, false if native wake lock.
 *
 * The stats below are specific to NATIVE wake locks and hold no valid
 * data in the context of kernel wake locks.
 *
 * @pid:                Pid of process that acquired native wake lock.
 *
 * The stats below are specific to KERNEL wake locks and hold no valid
 * data in the context of native wake locks.
 *
 * @eventCount:         Number of signaled wakeup events.
 * @expireCount:        Number times the wakeup source's timeout expired.
 * @preventSuspendTime: Total time this wake lock has been preventing autosuspend.
 * @wakeupCount:        Number of times the wakeup source might abort suspend.
 */
parcelable WakeLockInfo {
    @utf8InCpp String name;
    long activeCount;
    long lastChange;
    long maxTime;
    long totalTime;
    boolean isActive;
    long activeTime;
    boolean isKernelWakelock;

    // ---- Specific to Native Wake locks ---- //
    int pid;

    // ---- Specific to Kernel Wake locks ---- //
    long eventCount;
    long expireCount;
    long preventSuspendTime;
    long wakeupCount;
}
