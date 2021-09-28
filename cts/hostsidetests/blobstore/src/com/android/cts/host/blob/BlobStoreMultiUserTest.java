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
package com.android.cts.host.blob;

import static org.junit.Assume.assumeTrue;

import static com.google.common.truth.Truth.assertThat;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.util.Pair;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

@RunWith(DeviceJUnit4ClassRunner.class)
public class BlobStoreMultiUserTest extends BaseBlobStoreHostTest {
    private static final String TEST_CLASS = TARGET_PKG + ".DataCleanupTest";

    private int mPrimaryUserId;
    private int mSecondaryUserId;

    @Before
    public void setUp() throws Exception {
        assumeTrue("Multi-user is not supported on this device",
                isMultiUserSupported());

        mPrimaryUserId = getDevice().getPrimaryUserId();
        mSecondaryUserId = getDevice().createUser("Test_User");
        assertThat(getDevice().startUser(mSecondaryUserId)).isTrue();

        installPackageAsUser(TARGET_APK, true /* grantPermissions */, mPrimaryUserId);
        installPackageAsUser(TARGET_APK, true /* grantPermissions */, mSecondaryUserId);
    }

    @After
    public void tearDown() throws Exception {
        if (mSecondaryUserId > 0) {
            getDevice().removeUser(mSecondaryUserId);
        }
    }

    @Test
    public void testCreateAndOpenSession() throws Exception {
        // Create a session.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testCreateSession",
                mPrimaryUserId);
        final Map<String, String> args = createArgsFromLastTestRun();
        // Verify that previously created session can be accessed.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testOpenSession", args,
                mPrimaryUserId);
        // verify that previously created session cannot be accessed from another user.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testOpenSession_shouldThrow", args,
                mSecondaryUserId);
    }

    @Test
    public void testCommitAndOpenBlob() throws Exception {
        Map<String, String> args = createArgs(Pair.create(KEY_ALLOW_PUBLIC, String.valueOf(1)));
        // Commit a blob.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testCommitBlob", args,
                mPrimaryUserId);
        args = createArgsFromLastTestRun();
        // Verify that previously committed blob can be accessed.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testOpenBlob", args,
                mPrimaryUserId);
        // Verify that previously committed blob cannot be access from another user.
        runDeviceTestAsUser(TARGET_PKG, TEST_CLASS, "testOpenBlob_shouldThrow", args,
                mSecondaryUserId);
    }
}
