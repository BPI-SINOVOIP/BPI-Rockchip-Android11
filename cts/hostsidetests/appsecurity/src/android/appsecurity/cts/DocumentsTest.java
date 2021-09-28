/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.appsecurity.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;

import com.android.tradefed.device.DeviceNotAvailableException;

import com.google.common.collect.ImmutableSet;

/**
 * Set of tests that verify behavior of
 * {@link android.provider.DocumentsContract} and related intents.
 */
public class DocumentsTest extends DocumentsTestCase {
    private static final String PROVIDER_PKG = "com.android.cts.documentprovider";
    private static final String DUMMYIME_PKG = "com.android.cts.dummyime";
    private static final String PROVIDER_APK = "CtsDocumentProvider.apk";
    private static final String DUMMYIME_APK = "CtsDummyIme.apk";

    private static final long RESTRICT_STORAGE_ACCESS_FRAMEWORK = 141600225L;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        getDevice().uninstallPackage(PROVIDER_PKG);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        assertNull(getDevice().installPackage(buildHelper.getTestFile(PROVIDER_APK), false));
        assertNull(getDevice().installPackage(buildHelper.getTestFile(DUMMYIME_APK), false));
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        getDevice().uninstallPackage(PROVIDER_PKG);
        getDevice().uninstallPackage(DUMMYIME_PKG);
    }

    public void testOpenSimple() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testOpenSimple");
    }

    public void testOpenVirtual() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testOpenVirtual");
    }

    public void testCreateNew() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testCreateNew");
    }

    public void testCreateExisting() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testCreateExisting");
    }

    public void testTree() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testTree");
    }

    public void testGetContent_rootsShowing() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testGetContent_rootsShowing");
    }

    public void testGetContentWithQuery_matchingFileShowing() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest",
                "testGetContentWithQuery_matchingFileShowing");
    }

    public void testGetContent_returnsResultToCallingActivity() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest",
                "testGetContent_returnsResultToCallingActivity");
    }

    public void testTransferDocument() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testTransferDocument");
    }

    public void testFindDocumentPathInScopedAccess() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testFindDocumentPathInScopedAccess");
    }

    public void testOpenDocumentAtInitialLocation() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testOpenDocumentAtInitialLocation");
    }

    public void testOpenDocumentTreeAtInitialLocation() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testOpenDocumentTreeAtInitialLocation");
    }

    public void testOpenDocumentTreeWithScopedStorage() throws Exception {
        if (isAtLeastR()) {
            runDeviceTests(CLIENT_PKG, ".DocumentsClientTest",
                "testOpenDocumentTreeWithScopedStorage");
        }
    }

    public void testOpenRootWithoutRootIdAtInitialLocation() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest",
                "testOpenRootWithoutRootIdAtInitialLocation");
    }

    public void testCreateDocumentAtInitialLocation() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testCreateDocumentAtInitialLocation");
    }

    public void testCreateWebLink() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testCreateWebLink");
    }

    public void testEject() throws Exception {
        runDeviceTests(CLIENT_PKG, ".DocumentsClientTest", "testEject");
    }

    public void testRestrictStorageAccessFrameworkEnabled_blockFromTree() throws Exception {
        if (isAtLeastR() && isSupportedHardware()) {
            runDeviceCompatTest(CLIENT_PKG, ".DocumentsClientTest",
                "testRestrictStorageAccessFrameworkEnabled_blockFromTree",
                /* enabledChanges */ ImmutableSet.of(RESTRICT_STORAGE_ACCESS_FRAMEWORK),
                /* disabledChanges */ ImmutableSet.of());
        }
    }

    public void testRestrictStorageAccessFrameworkDisabled_notBlockFromTree() throws Exception {
        if (isAtLeastR() && isSupportedHardware()) {
            runDeviceCompatTest(CLIENT_PKG, ".DocumentsClientTest",
                "testRestrictStorageAccessFrameworkDisabled_notBlockFromTree",
                /* enabledChanges */ ImmutableSet.of(),
                /* disabledChanges */ ImmutableSet.of(RESTRICT_STORAGE_ACCESS_FRAMEWORK));
        }
    }

    private boolean isAtLeastR() {
        try {
            String apiString = getDevice().getProperty("ro.build.version.sdk");
            if (apiString == null) {
                return false;
            }
            int apiLevel = Integer.parseInt(apiString);
            return apiLevel > 29;
        } catch (Exception e) {
            return false;
        }
    }

    private boolean isSupportedHardware() {
        try {
            if (getDevice().hasFeature("feature:android.hardware.type.television")
                    || getDevice().hasFeature("feature:android.hardware.type.watch")
                    || getDevice().hasFeature("feature:android.hardware.type.automotive")) {
                return false;
            }
        } catch (DeviceNotAvailableException e) {
            return true;
        }
        return true;
    }
}
