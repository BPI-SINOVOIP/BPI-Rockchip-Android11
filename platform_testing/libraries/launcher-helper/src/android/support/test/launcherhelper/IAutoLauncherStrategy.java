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
package android.support.test.launcherhelper;

import android.app.Instrumentation;

public interface IAutoLauncherStrategy extends ILauncherStrategy {

    /**
     * Sets an instance of instrumentation
     */
    void setInstrumentation(Instrumentation instrumentation);

    /**
     * Open Home Facet.
     */
    void openHomeFacet();

    /**
     * Open Maps Facet.
     */
    void openMapsFacet();

    /**
     * Open Media Facet.
     */
    void openMediaFacet();

    /**
     * Open Media Facet.
     * @param appName open app from media facet.
     */
    void openMediaFacet(String appName);

    /**
     * Open Dial Facet.
     */
    void openDialFacet();

    /**
     * Open App Grid Facet.
     */
    void openAppGridFacet();

    /**
     * Open Notification Facet.
     */
    void openNotificationFacet();

    /**
     * Open Google Assistant Facet.
     */
    void openAssistantFacet();

    /** This method is to open Bluetooth Audio application */
    void openBluetoothAudioApp();

    /** This method is to open Google Play Store application */
    void openGooglePlayStore();

    /** Open Quick Settings. */
    void openQuickSettings();

    /**
     * Click on left HVAC.
     */
    void clickLeftHvac();

    /**
     * Click on right HVAC.
     */
    void clickRightHvac();

    /**
     * This method is to check if an application is visible on UI
     *
     * @param appName check app from all apps facet.
     */
    boolean checkApplicationExists(String appName);

    /**
     * This method is to open an application if it appears on UI
     *
     * It throws an exception when the target application is not found
     *
     * @param appName application to be opened.
     */
    void openApp(String appName);

    /** This method is to open notifications */
    void openNotifications();

    /** This method is to navigate to device home */
    void pressHome();
}
