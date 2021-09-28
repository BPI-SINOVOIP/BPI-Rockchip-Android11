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

package android.platform.helpers;

/** Helper class for functional tests of Security settings */
public interface IAutoSecuritySettingsHelper extends IAppHelper {
    /**
     * Setup expectation: Security setting is open.
     *
     * <p>This method is to set password using password lock type.
     *
     * @param password - input password
     */
    void setLockByPassword(String password);

    /**
     * Setup expectation: Security setting is open.
     *
     * <p>This method is to set PIN using PIN lock type.
     *
     * @param password - input PIN
     */
    void setLockByPin(String pin);

    /**
     * Setup expectation: Security setting is open.
     *
     * <p>This method is to check if the device is locked.
     */
    boolean isDeviceLocked();

    /**
     * Setup expectation: Security setting is open.
     *
     * <p>This method is to unlock the device by password.
     *
     * @param password - input password
     */
    void unlockByPassword(String password);

    /**
     * Setup expectation: Security setting is open.
     *
     * <p>This method is to unlock the device by pin.
     *
     * @param password - input PIN
     */
    void unlockByPin(String pin);

    /**
     * Setup expectation: Security setting is open and device is unlocked.
     *
     * <p>This method is to remove any lock, i.e. set the device lock to none.
     */
    void removeLock();
}
