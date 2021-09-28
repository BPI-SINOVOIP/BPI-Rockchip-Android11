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

import static com.android.cts.net.hostside.NetworkPolicyTestUtils.resetMeteredNetwork;
import static com.android.cts.net.hostside.NetworkPolicyTestUtils.setupMeteredNetwork;
import static com.android.cts.net.hostside.Property.METERED_NETWORK;
import static com.android.cts.net.hostside.Property.NON_METERED_NETWORK;

import android.util.ArraySet;
import android.util.Pair;

import com.android.compatibility.common.util.BeforeAfterRule;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

public class MeterednessConfigurationRule extends BeforeAfterRule {
    private Pair<String, Boolean> mSsidAndInitialMeteredness;

    @Override
    public void onBefore(Statement base, Description description) throws Throwable {
        final ArraySet<Property> requiredProperties
                = RequiredPropertiesRule.getRequiredProperties();
        if (requiredProperties.contains(METERED_NETWORK)) {
            configureNetworkMeteredness(true);
        } else if (requiredProperties.contains(NON_METERED_NETWORK)) {
            configureNetworkMeteredness(false);
        }
    }

    @Override
    public void onAfter(Statement base, Description description) throws Throwable {
        resetNetworkMeteredness();
    }

    public void configureNetworkMeteredness(boolean metered) throws Exception {
        mSsidAndInitialMeteredness = setupMeteredNetwork(metered);
    }

    public void resetNetworkMeteredness() throws Exception {
        if (mSsidAndInitialMeteredness != null) {
            resetMeteredNetwork(mSsidAndInitialMeteredness.first,
                    mSsidAndInitialMeteredness.second);
        }
    }
}
