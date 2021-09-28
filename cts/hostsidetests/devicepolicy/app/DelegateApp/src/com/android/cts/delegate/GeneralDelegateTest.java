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
package com.android.cts.delegate;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.os.Bundle;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;

import androidx.test.InstrumentationRegistry;

import java.util.List;

/**
 * Test general properties of delegate applications that should apply to any delegation scope
 * granted by a device or profile owner via {@link DevicePolicyManager#setDelegatedScopes}.
 */
public class GeneralDelegateTest extends InstrumentationTestCase {

    private DevicePolicyManager mDpm;
    private static final String PARAM_SCOPES = "scopes";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDpm = getInstrumentation().getContext().getSystemService(DevicePolicyManager.class);
    }

    public void testGetsExpectedDelegationScopes() {
        Bundle arguments = InstrumentationRegistry.getArguments();
        String[] expectedScopes = arguments.getString(PARAM_SCOPES).split(",");
        List<String> delegatedScopes = mDpm.getDelegatedScopes(null,
                getInstrumentation().getContext().getPackageName());

        assertNotNull("Received null scopes", delegatedScopes);
        MoreAsserts.assertContentsInAnyOrder("Delegated scopes do not match expected scopes",
                delegatedScopes, expectedScopes);
    }

    public void testDifferentPackageNameThrowsException() {
        final String otherPackage = "com.android.cts.launcherapps.simpleapp";
        try {
            List<String> delegatedScopes = mDpm.getDelegatedScopes(null, otherPackage);
            fail("Expected SecurityException not thrown");
        } catch (SecurityException expected) {
            MoreAsserts.assertContainsRegex("Caller with uid \\d+ is not " + otherPackage,
                    expected.getMessage());
        }
    }

    public void testSettingAdminComponentNameThrowsException() {
        final String myPackageName = getInstrumentation().getContext().getPackageName();
        final ComponentName myComponentName = new ComponentName(myPackageName,
                GeneralDelegateTest.class.getName());

        try {
            mDpm.setUninstallBlocked(myComponentName, myPackageName, true);
            fail("Expected SecurityException not thrown");
        } catch (SecurityException expected) {
            MoreAsserts.assertContainsRegex("No active admin", expected.getMessage());
        }
    }
}
