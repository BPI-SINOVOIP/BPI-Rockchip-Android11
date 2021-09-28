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

/**
 * Helper class for functional tests of system settings
 */

import java.util.Date;

public interface IAutoSystemSettingsHelper extends IAppHelper {
    /**
     * Setup expectation: System setting is open.
     *
     * Set display language.
     *
     * @param language - input language.
     */
    void setDisplayLanguage(String language);

    /**
     * Setup expectation: System setting is open.
     *
     * Get current display language.
     */
    String getCurrentLanguage();

    /**
     * Setup expectation: System setting is open.
     *
     * Get device model number from UI.
     */
    String getDeviceModel();

    /**
     * Setup expectation: System setting is open.
     *
     * Get android version from UI.
     */
    String getAndroidVersion();

    /**
     * Setup expectation: System setting is open.
     *
     * Get android security patch level from UI.
     */
    Date getAndroidSecurityPatchLevel();

    /**
     * Setup expectation: System setting is open.
     *
     * Get kernel version from UI.
     */
    String getKernelVersion();

    /**
     * Setup expectation: System setting is open.
     *
     * Get build number from UI.
     */
    String getBuildNumber();

    /**
     * Setup expectation: System setting is open.
     *
     * Reset network connection [ Wifi & Bluetooth ].
     */
    void resetNetwork();


    /**
     * Setup expectation: System setting is open.
     *
     * Reset application preferences.
     */
    void resetAppPreferences();

    /**
     * Setup expectation: System setting is open.
     *
     * Open Languages & input menu.
     */
    void openLanguagesInputMenu();
}
