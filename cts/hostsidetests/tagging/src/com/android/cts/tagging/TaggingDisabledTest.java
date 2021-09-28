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

package com.android.cts.tagging;

import com.android.cts.tagging.TaggingBaseTest;

import com.google.common.collect.ImmutableSet;

public class TaggingDisabledTest extends TaggingBaseTest {

    protected static final String TEST_APK = "CtsHostsideTaggingDisabledApp.apk";
    protected static final String TEST_PKG = "android.cts.tagging.disabled";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        installPackage(TEST_APK, true);
    }

    @Override
    protected void tearDown() throws Exception {
        uninstallPackage(TEST_PKG, true);
    }

    public void testCompatFeatureEnabled() throws Exception {
        // Checking for supportsTaggedPointers is unnecessary here, as we don't
        // validate against reportDisabledChanges.
        runDeviceCompatTestReported(TEST_PKG, ".TaggingTest", "testHeapTaggingDisabled",
                /*enabledChanges*/ImmutableSet.of(NATIVE_HEAP_POINTER_TAGGING_CHANGE_ID),
                /*disabledChanges*/ ImmutableSet.of(),
                /*reportedEnabledChanges*/ ImmutableSet.of(),
                /*reportedDisabledChanges*/ ImmutableSet.of());
    }

    public void testCompatFeatureDisabled() throws Exception {
        // Checking for supportsTaggedPointers is unnecessary here, as we don't
        // validate against reportDisabledChanges.
        runDeviceCompatTestReported(TEST_PKG, ".TaggingTest", "testHeapTaggingDisabled",
                /*enabledChanges*/ImmutableSet.of(),
                /*disabledChanges*/ ImmutableSet.of(NATIVE_HEAP_POINTER_TAGGING_CHANGE_ID),
                /*reportedEnabledChanges*/ ImmutableSet.of(),
                /*reportedDisabledChanges*/ ImmutableSet.of());
    }
}
