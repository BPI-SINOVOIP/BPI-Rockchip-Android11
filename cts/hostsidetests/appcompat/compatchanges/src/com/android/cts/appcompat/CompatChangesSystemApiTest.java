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

package com.android.cts.appcompat;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.compat.cts.CompatChangeGatingTestCase;

import com.google.common.collect.ImmutableSet;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests for the {@link android.app.compat.CompatChanges} SystemApi.
 */

public class CompatChangesSystemApiTest extends CompatChangeGatingTestCase {

    protected static final String TEST_APK = "CtsHostsideCompatChangeTestsApp.apk";
    protected static final String TEST_PKG = "com.android.cts.appcompat";

    private static final long CTS_SYSTEM_API_CHANGEID = 149391281;

    @Override
    protected void setUp() throws Exception {
        installPackage(TEST_APK, true);
    }

    @Override
    protected void tearDown() throws Exception {
        uninstallPackage(TEST_PKG, true);
    }

    public void testIsChangeEnabled() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest", "isChangeEnabled_changeEnabled",
                /*enabledChanges*/ImmutableSet.of(CTS_SYSTEM_API_CHANGEID),
                /*disabledChanges*/ ImmutableSet.of());
    }

    public void testIsChangeEnabledPackageName() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest",
                "isChangeEnabledPackageName_changeEnabled",
                /*enabledChanges*/ImmutableSet.of(CTS_SYSTEM_API_CHANGEID),
                /*disabledChanges*/ ImmutableSet.of());
    }

    public void testIsChangeEnabledUid() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest", "isChangeEnabledUid_changeEnabled",
                /*enabledChanges*/ImmutableSet.of(CTS_SYSTEM_API_CHANGEID),
                /*disabledChanges*/ ImmutableSet.of());
    }

    public void testIsChangeDisabled() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest", "isChangeEnabled_changeDisabled",
                /*enabledChanges*/ImmutableSet.of(),
                /*disabledChanges*/ ImmutableSet.of(CTS_SYSTEM_API_CHANGEID));
    }

    public void testIsChangeDisabledPackageName() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest",
                "isChangeEnabledPackageName_changeDisabled",
                /*enabledChanges*/ImmutableSet.of(),
                /*disabledChanges*/ ImmutableSet.of(CTS_SYSTEM_API_CHANGEID));
    }

    public void testIsChangeDisabledUid() throws Exception {
        runDeviceCompatTest(TEST_PKG, ".CompatChangesTest", "isChangeEnabledUid_changeDisabled",
                /*enabledChanges*/ImmutableSet.of(),
                /*disabledChanges*/ ImmutableSet.of(CTS_SYSTEM_API_CHANGEID));
    }
}
