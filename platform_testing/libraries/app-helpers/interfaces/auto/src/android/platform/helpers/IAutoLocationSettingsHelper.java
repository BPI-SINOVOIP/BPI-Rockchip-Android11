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

public interface IAutoLocationSettingsHelper extends IAppHelper {
    /** enum class for State. */
    enum State {
        ON,
        OFF
    }

    /**
     * Setup expectations: Location setting is open
     *
     * <p>Toggle on/off location switch
     *
     * @param state of toggle switch.
     */
    void toggleLocationSwitchOnOff(State state);

    /**
     * Setup expectations: Location setting is open
     *
     * <p>Check if location switch is toggled on.
     */
    boolean isLocationSwitchOn();

    /**
     * Setup expectations: None
     *
     * <p>Get device longitude.
     */
    double getLongitude();

    /**
     * Setup expectations: None
     *
     * <p>get device latitude.
     */
    double getLatitude();
}
