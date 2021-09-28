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

package android.platform.helpers;

/** Helper class for functional tests of Lock Screen */
public interface IAutoLockScreenHelper extends IAppHelper {

    /** Enum class for supported lock type. */
    enum LockType {
        PIN,
        PASSWORD
    }
    /**
     * Setup expectation: none.
     *
     * <p>This method is to lock device screen.
     *
     * @param lockType is the lock type used to lock screen.
     * @param credential is the pin/password used.
     */
    void lockScreenBy(LockType lockType, String credential);

    /**
     * Setup expectation: none.
     *
     * <p>This method is to unlock device screen.
     *
     * @param lockType is the lock type used to lock screen.
     * @param credential is the pin/password used.
     */
    void unlockScreenBy(LockType lockType, String credential);
}
