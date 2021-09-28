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

/** An App Helper interface for the Quick Settings bar. */
public interface IQuickSettingsHelper extends IAppHelper {
    /**
     * Represents the state of a Quick Setting. Currently this is limited to ON and OFF states;
     * however, other states will be added in the future, for example to differentiate between
     * paired and un-paired, active bluetooth states.
     */
    public enum State {
        ON,
        OFF,
    }

    /** Represents a Quick Setting switch that can be toggled ON and OFF during a test. */
    public enum Setting {
        AIRPLANE("Airplane", 2000),
        AUTO_ROTATE("Auto-rotate", 2000),
        BLUETOOTH("Bluetooth", 15000),
        DO_NOT_DISTURB("Do Not Disturb", 2000),
        FLASHLIGHT("Flashlight", 5000),
        NIGHT_LIGHT("Night Light", 2000),
        WIFI("Wi-Fi", 10000);

        private final String mContentDescSubstring;
        private final long mExpectedWait;

        Setting(String substring, long wait) {
            mContentDescSubstring = substring;
            mExpectedWait = wait;
        }

        /** Returns a substring to identify the {@code Setting} by content description. */
        public String getContentDescSubstring() {
            return mContentDescSubstring;
        }

        /** Returns the longest expected wait time for this option to be toggled ON or OFF. */
        public long getExpectedWait() {
            return mExpectedWait;
        }
    }

    /**
     * Toggles a {@link Setting} either {@link State.ON} or {@link State.OFF}. If {@code setting} is
     * already found to be in {@code state}, then no operation is performed. There are no setup
     * requirements to call this method, except that {@code setting} is available from the test and
     * in the Quick Settings menu.
     */
    public void toggleSetting(Setting setting, State state);
}
