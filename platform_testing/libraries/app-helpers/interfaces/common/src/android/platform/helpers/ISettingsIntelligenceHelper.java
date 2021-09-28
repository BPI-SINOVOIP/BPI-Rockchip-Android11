/*
 * Copyright (C) 2017 The Android Open Source Project
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

public interface ISettingsIntelligenceHelper extends IAppHelper {

    public static final String PAGE_ACTION_HOME = "";
    public static final String PAGE_ACTION_APPLICATION = "android.settings.APPLICATION_SETTINGS";
    public static final String PAGE_ACTION_BATTERY = "android.intent.action.POWER_USAGE_SUMMARY";
    public static final String PAGE_ACTION_BLUETOOTH = "android.settings.BLUETOOTH_SETTINGS";
    public static final String PAGE_ACTION_LOCATION = "android.settings.LOCATION_SOURCE_SETTINGS";
    public static final String PAGE_ACTION_STORAGE = "android.settings.INTERNAL_STORAGE_SETTINGS";
    public static final String PAGE_ACTION_WIFI = "android.settings.WIFI_SETTINGS";

    /**
     * Sets the action representing the Settings page to open when open() is called.
     *
     * @param pageAction One of the PAGE_ACTION* constants.
     */
    void setPageAction(String pageAction);

    /**
     * Setup expectations: Settings search page is open
     *
     * This method performs a text query and expects to see "no result" image.
     */
    void performQuery();

    /**
     * Setup expectations: Settings homepage is open
     *
     * <p>This method opens search page.
     */
    void openSearch();
}