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

package android.telephony.cts;

import android.compat.cts.CompatChangeGatingTestCase;

import com.google.common.collect.ImmutableSet;

/**
 * Tests for {@link android.telephony.TelephonyManager} and related APIs
 */

public class TelephonyHostTest extends CompatChangeGatingTestCase {

    protected static final String TEST_APK = "TelephonyDeviceTest.apk";
    protected static final String TEST_PKG = "android.telephony.device.cts";

    public static final long PHONE_STATE_LISTENER_LIMIT_CHANGE_ID = 150880553L;

    @Override
    protected void setUp() throws Exception {
        installPackage(TEST_APK, true);
    }

    public void testWithChangeEnabled() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".TelephonyTest", "testListenerRegistrationWithChangeEnabled",
                /*enabledChanges*/ImmutableSet.of(PHONE_STATE_LISTENER_LIMIT_CHANGE_ID),
                /*disabledChanges*/ ImmutableSet.of());
    }
}
