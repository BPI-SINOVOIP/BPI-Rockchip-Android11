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

package com.android.cts.delegate;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.test.InstrumentationTestCase;

/**
 *  A helper for testing the {@link
 *  DevicePolicyManager#isCallerApplicationRestrictionsManagingPackage()} method.
 *  <p>The method names start with "test" to be recognized by {@link InstrumentationTestCase}.
 */
public class AppRestrictionsIsCallerDelegateHelper extends InstrumentationTestCase {

    private DevicePolicyManager mDevicePolicyManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDevicePolicyManager =
            getInstrumentation().getContext().getSystemService(DevicePolicyManager.class);
    }

    public void testAssertCallerIsApplicationRestrictionsManagingPackage() {
        assertThat(mDevicePolicyManager.isCallerApplicationRestrictionsManagingPackage()).isTrue();
    }

    public void testAssertCallerIsNotApplicationRestrictionsManagingPackage() {
        assertThat(mDevicePolicyManager.isCallerApplicationRestrictionsManagingPackage()).isFalse();
    }
}
