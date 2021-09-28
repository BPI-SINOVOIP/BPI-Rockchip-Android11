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
package com.android.cts.net.hostside;

import static com.android.cts.net.hostside.NetworkPolicyTestUtils.canChangeActiveNetworkMeteredness;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isActiveNetworkMetered;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isAppStandbySupported;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isBatterySaverSupported;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isDataSaverSupported;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isDozeModeSupported;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.isLowRamDevice;

public enum Property {
    BATTERY_SAVER_MODE(1 << 0) {
        public boolean isSupported() { return isBatterySaverSupported(); }
    },

    DATA_SAVER_MODE(1 << 1) {
        public boolean isSupported() { return isDataSaverSupported(); }
    },

    NO_DATA_SAVER_MODE(~DATA_SAVER_MODE.getValue()) {
        public boolean isSupported() { return !isDataSaverSupported(); }
    },

    DOZE_MODE(1 << 2) {
        public boolean isSupported() { return isDozeModeSupported(); }
    },

    APP_STANDBY_MODE(1 << 3) {
        public boolean isSupported() { return isAppStandbySupported(); }
    },

    NOT_LOW_RAM_DEVICE(1 << 4) {
        public boolean isSupported() { return !isLowRamDevice(); }
    },

    METERED_NETWORK(1 << 5) {
        public boolean isSupported() {
            return isActiveNetworkMetered(true) || canChangeActiveNetworkMeteredness();
        }
    },

    NON_METERED_NETWORK(~METERED_NETWORK.getValue()) {
        public boolean isSupported() {
            return isActiveNetworkMetered(false) || canChangeActiveNetworkMeteredness();
        }
    };

    private int mValue;

    Property(int value) { mValue = value; }

    public int getValue() { return mValue; }

    abstract boolean isSupported();
}
