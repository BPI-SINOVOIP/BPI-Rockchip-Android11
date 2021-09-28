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

package android.platform.helpers;

public interface IClockHelper extends IAppHelper {
    /**
     * Setup expectations: Clock is open.
     *
     * Clicks Alarm to go to Alarm page.
     *
     */
    default public void goToAlarms() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Clock is open.
     *
     * Clicks Clock to go to Clock page.
     *
     */
    default public void goToClock() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Clock is open.
     *
     * Clicks Timer to go to Timer page.
     *
     */
    default public void goToTimer() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
