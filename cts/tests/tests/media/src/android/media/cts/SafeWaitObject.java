/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.media.cts;

/**
 * Helper class to simplify the handling of spurious wakeups in Object.wait()
 */
final class SafeWaitObject {
    private boolean mQuit = false;

    public void safeNotify() {
        synchronized (this) {
            mQuit = true;
            this.notify();
        }
    }

    public void safeWait(long millis) throws InterruptedException {
        final long timeOutTime = java.lang.System.currentTimeMillis() + millis;
        synchronized (this) {
            while (!mQuit) {
                final long timeToWait = timeOutTime - java.lang.System.currentTimeMillis();
                if (timeToWait < 0) {
                    break;
                }
                this.wait(timeToWait);
            }
        }
    }
}
