/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.car.settings;

import android.annotation.SystemApi;

/**
 * System-level, car-related settings.
 *
 * @hide
 */
@SystemApi
public class CarSettings {

    private CarSettings() {
        throw new UnsupportedOperationException("this class only provide constants");
    }

    /**
     * Global car settings, containing preferences that always apply identically
     * to all defined users.  Applications can read these but are not allowed to write;
     * like the "Secure" settings, these are for preferences that the user must
     * explicitly modify through the system UI or specialized APIs for those values.
     *
     * <p>To read/write the global car settings, use {@link android.provider.Settings.Global}
     * with the keys defined here.
     *
     * @hide
     */
    public static final class Global {

        private Global() {
            throw new UnsupportedOperationException("this class only provide constants");
        }

        /**
         * Whether default restrictions for users have been set.
         *
         * @hide
         */
        public static final String DEFAULT_USER_RESTRICTIONS_SET =
                "android.car.DEFAULT_USER_RESTRICTIONS_SET";

        /**
         * Developer settings String used to explicitly disable the instrumentation service (when
         * set to {@code "true"}.
         *
         * @hide
         */
        public static final String DISABLE_INSTRUMENTATION_SERVICE =
                "android.car.DISABLE_INSTRUMENTATION_SERVICE";

        /**
         * Developer settings String used to explicitly enable the user switch message when
         * set to {@code "true"}.
         *
         * @hide
         */
        public static final String ENABLE_USER_SWITCH_DEVELOPER_MESSAGE =
                "android.car.ENABLE_USER_SWITCH_DEVELOPER_MESSAGE";

        /**
         * User id of the last foreground user
         *
         * @hide
         */
        public static final String LAST_ACTIVE_USER_ID =
                        "android.car.LAST_ACTIVE_USER_ID";

        /**
         * User id of the last persistent (i.e, not counting ephemeral guests) foreground user
         *
         * @hide
         */
        public static final String LAST_ACTIVE_PERSISTENT_USER_ID =
                        "android.car.LAST_ACTIVE_PERSISTENT_USER_ID";

        /**
         * Defines global runtime overrides to system bar policy.
         *
         * See {@link com.android.systemui.wm.BarControlPolicy} for value format.
         *
         * @hide
         */
        public static final String SYSTEM_BAR_VISIBILITY_OVERRIDE =
                "android.car.SYSTEM_BAR_VISIBILITY_OVERRIDE";
    }

    /**
     * Default garage mode wake up time 00:00
     *
     * @hide
     */
    public static final int[] DEFAULT_GARAGE_MODE_WAKE_UP_TIME = {0, 0};

    /**
     * Default garage mode maintenance window 10 mins.
     *
     * @hide
     */
    public static final int DEFAULT_GARAGE_MODE_MAINTENANCE_WINDOW = 10 * 60 * 1000; // 10 mins

    /**
     * @hide
     */
    @SystemApi
    public static final class Secure {

        private Secure() {
            throw new UnsupportedOperationException("this class only provide constants");
        }

        /**
         * Key to indicate whether audio focus requests for
         * {@link android.hardware.automotive.audiocontrol.V1_0.ContextNumber.NAVIGATION} should
         * be rejected if focus is currently held by
         * {@link android.hardware.automotive.audiocontrol.V1_0.ContextNumber.CALL}.
         * <p>The value is a boolean (1 or 0) where:
         * <ul>
         * <li>1 indicates {@code NAVIGATION} should be rejected when a {@code CALL} is in progress.
         * <li>0 indicates {@code NAVIGATION} and {@code CALL} should be allowed to hold focus
         * concurrently.
         * </ul>
         *
         * <p>Recommended {@code false} as default value.
         *
         * @hide
         */
        @SystemApi
        public static final String KEY_AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL =
                "android.car.KEY_AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL";

        /**
         * Key for a list of devices to automatically connect on Bluetooth A2DP Sink profile
         * Written to and read by {@link com.android.car.BluetoothDeviceConnectionPolicy}
         * @hide
         */
        public static final String KEY_BLUETOOTH_A2DP_SINK_DEVICES =
                "android.car.KEY_BLUETOOTH_A2DP_SINK_DEVICES";

        /**
         * Key for a list of devices to automatically connect on Bluetooth PAN profile
         * Written to and read by {@link com.android.car.BluetoothDeviceConnectionPolicy}
         * @hide
         */
        public static final String KEY_BLUETOOTH_PAN_DEVICES =
                "android.car.KEY_BLUETOOTH_PAN_DEVICES";

        /**
         * Key for a list of devices to automatically connect on Bluetooth HFP Client profile
         * Written to and read by {@link com.android.car.BluetoothDeviceConnectionPolicy}
         * @hide
         */
        public static final String KEY_BLUETOOTH_HFP_CLIENT_DEVICES =
                "android.car.KEY_BLUETOOTH_HFP_CLIENT_DEVICES";

        /**
         * Key for a list of devices to automatically connect on Bluetooth MAP Client profile
         * Written to and read by {@link com.android.car.BluetoothDeviceConnectionPolicy}
         * @hide
         */
        public static final String KEY_BLUETOOTH_MAP_CLIENT_DEVICES =
                "android.car.KEY_BLUETOOTH_MAP_CLIENT_DEVICES";

        /**
         * Key for a list of devices to automatically connect on Bluetooth PBAP Client profile
         * Written to and read by {@link com.android.car.BluetoothDeviceConnectionPolicy}
         * @hide
         */
        public static final String KEY_BLUETOOTH_PBAP_CLIENT_DEVICES =
                "android.car.KEY_BLUETOOTH_PBAP_CLIENT_DEVICES";

        /**
         * Key for storing temporarily-disconnected devices and profiles.
         * Read and written by {@link com.android.car.BluetoothDeviceConnectionPolicy}.
         * @hide
         */
        public static final String KEY_BLUETOOTH_PROFILES_INHIBITED =
                "android.car.BLUETOOTH_PROFILES_INHIBITED";

        /**
         * Key to enable / disable initial notice screen that will be shown for all user-starting
         * moments including cold boot, wake up from suspend, and user switching.
         * The value is boolean (1 or 0).
         * @hide
         */
        public static final String KEY_ENABLE_INITIAL_NOTICE_SCREEN_TO_USER =
                "android.car.ENABLE_INITIAL_NOTICE_SCREEN_TO_USER";

        /**
         * Key to indicate Setup Wizard is in progress. It differs from USER_SETUP_COMPLETE in
         * that this flag can be reset to 0 in deferred Setup Wizard flow.
         * The value is boolean (1 or 0).
         * @hide
         */
        public static final String KEY_SETUP_WIZARD_IN_PROGRESS =
                "android.car.SETUP_WIZARD_IN_PROGRESS";
    }
}
