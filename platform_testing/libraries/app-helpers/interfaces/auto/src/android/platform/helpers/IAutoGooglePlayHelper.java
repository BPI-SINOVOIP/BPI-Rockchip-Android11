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

public interface IAutoGooglePlayHelper extends IAppHelper, Scrollable {

    /**
     * Setup expectations: Launch Google Play Store app.
     *
     * <p>This method is used to Open Google Play Store app.
     */
    void openGooglePlayStore();

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to search an app and click it in Google Play.
     */
    void searchAndClick(String appName);

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to search an app.
     */
    void searchApp(String appName);

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to install a app.
     */
    void installApp();

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to cancel a download.
     */
    void cancelDownload();

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to return back to Google Play main page
     */
    void returnToMainPage();

    /**
     * Setup expectations: Google Play app is open.
     *
     * <p>This method is used to open a installed app.
     */
    @Deprecated
    void openApp();

    /**
     * Setup expectations: None.
     *
     * <p>This method is used to check if the given application package is installed.
     */
    boolean checkIfApplicationExists(String packageName);
}
