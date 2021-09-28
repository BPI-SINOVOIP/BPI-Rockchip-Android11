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

public interface IAutoNavigationBarHelper extends IAppHelper {
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Home page using Facet.
     */
    void openHomeFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Maps application using Facet.
     */
    void openMapsFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Media application using Facet.
     */
    void openMediaFacet();
    /**
     * Setup expectation: Media Application is open.
     *
     * <p>This method is to open given Media application.
     */
    void openMediaFacet(String appName);
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Dial application using Facet.
     */
    void openDialFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open App Grid using Facet.
     */
    void openAppGridFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Notifications using Facet.
     */
    void openNotificationsFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Assistant application using Facet.
     */
    void openAssistantFacet();
    /**
     * Setup expectation: None.
     *
     * <p>This method is to open Quick Settings.
     */
    void openQuickSettings();
}
