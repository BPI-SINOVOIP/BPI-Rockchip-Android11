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

package android.telephonyprovider.cts;

import android.compat.cts.CompatChangeGatingTestCase;

import com.google.common.collect.ImmutableSet;

/**
 * Tests for the {@link android.app.compat.CompatChanges} SystemApi.
 */

public class TelephonyProviderHostTest extends CompatChangeGatingTestCase {

    protected static final String TEST_APK = "TelephonyProviderDeviceTest.apk";
    protected static final String TEST_PKG = "android.telephonyprovider.device.cts";

    private static final long APN_READING_PERMISSION_CHANGE_ID = 124107808L;
    private static final String FEATURE_TELEPHONY = "android.hardware.telephony";

    @Override
    protected void setUp() throws Exception {
        if (!getDevice().hasFeature(FEATURE_TELEPHONY)) {
            return;
        }
        installPackage(TEST_APK, true);
    }

    public void testWithChangeEnabled() throws Exception {
        if (!getDevice().hasFeature(FEATURE_TELEPHONY)) {
            return;
        }
        runDeviceCompatTest(TEST_PKG, ".TelephonyProviderTest", "testAccessToApnsWithChangeEnabled",
                /*enabledChanges*/ImmutableSet.of(APN_READING_PERMISSION_CHANGE_ID),
                /*disabledChanges*/ ImmutableSet.of());
    }

    public void testWithChangeDisabled() throws Exception {
        if (!getDevice().hasFeature(FEATURE_TELEPHONY)) {
            return;
        }
        runDeviceCompatTest(TEST_PKG, ".TelephonyProviderTest",
                "testAccessToApnsWithChangeDisabled",
                /*enabledChanges*/ImmutableSet.of(),
                /*disabledChanges*/ ImmutableSet.of(APN_READING_PERMISSION_CHANGE_ID));
    }
}
