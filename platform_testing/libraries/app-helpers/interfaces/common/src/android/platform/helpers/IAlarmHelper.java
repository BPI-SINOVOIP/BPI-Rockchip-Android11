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

public interface IAlarmHelper extends IAppHelper {
    public enum AmPm {
        AM("am_label"),
        PM("pm_label");

        private String mDisplayName;

        AmPm(String displayName) {
            mDisplayName = displayName;
        }

        @Override
        public String toString() {
            return mDisplayName;
        }
    }

    /**
     * Setup expectation: Alarm app is open
     *
     * Create alarm
     */
    public void createAlarm();

    /**
     * Setup expectation: Alarm app is open
     *
     * Delete alarm
     */
    public void deleteAlarm();

    /**
     * Setup expectation: Alarm is ringing
     *
     * Dismiss alarm
     */
    public void dismissAlarm();

    /**
     * Setup expectations: on alarm time picker page.
     *
     * Sets alarm by the specific hour, minute and AM/PM. The minute will be adjusted to a multiple
     * of five that is less than or equal to the argument since only the multiple of five displayed.
     *
     * @param hour The hour of alarm to set.
     * @param minute The minute of alarm to set.
     * @param amPm The AM/PM of alarm to set.
     *
     */
    default public void setAlarmTime(int hour, int minute, IAlarmHelper.AmPm amPm) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: on Alarm page.
     *
     * Expands the specific alarm.
     *
     * @param index The index of alarm to expand.
     *
     */
    default public void selectAlarm(int index) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: on Alarm page.
     *
     * Checks if an alarm exists or not.
     *
     * @return true if an alarm exists, false otherwise.
     *
     */
    default public Boolean hasAlarm() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: on Alarm page and an alarm is on.
     *
     * Clicks the switch button to turn off the alarm.
     *
     */
    default public void setAlarmOff() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
