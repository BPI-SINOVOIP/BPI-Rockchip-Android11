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
 * Helper class for functional tests of date & time
 */

import java.time.LocalDate;

public interface IAutoDateTimeSettingsHelper extends IAppHelper {
    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Set the device date.
     *
     * @param date - input LocalDate object
     */
    void setDate(LocalDate date);

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Get the current date displayed on the UI in LocalDate object.
     */
    LocalDate getDate();

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Set the device time in 12-hour format
     *
     * @param hour - input hour
     * @param minute - input minute
     * @param is_am - input am/pm
     */
    void setTimeInTwelveHourFormat(int hour, int minute, boolean is_am);

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Set the device time in 24-hour format
     *
     * @param hour - input hour
     * @param minute - input minute
     */
    void setTimeInTwentyFourHourFormat(int hour, int minute);

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Get the current time displayed on the UI.
     *
     * @return returned time format will match the UI format exactly
     */
    String getTime();

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Set the device time zone.
     *
     * @param timezone - city selected for timezone
     */
    void setTimeZone(String timezone);

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Get the current timezone displayed on the UI.
     */
    String getTimeZone();

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Check if the 24 hour format is enabled
     */
    boolean isTwentyFourHourFormatEnabled();

    /**
     * Setup expectation: Date & time setting is open
     *
     * <p>Toggle on/off 24 hour format widget switch.
     */
    boolean toggleTwentyFourHourFormatSwitch();
}
