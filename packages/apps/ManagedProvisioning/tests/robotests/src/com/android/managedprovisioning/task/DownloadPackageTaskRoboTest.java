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
 * limitations under the License
 */

package com.android.managedprovisioning.task;

import static android.provider.Settings.Secure.MANAGED_PROVISIONING_DPC_DOWNLOADED;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.robolectric.Shadows.shadowOf;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.provider.Settings;

import com.android.managedprovisioning.model.PackageDownloadInfo;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

/** Test class for {@link DownloadPackaageTask}. */
@RunWith(RobolectricTestRunner.class)
public class DownloadPackageTaskRoboTest {

    private static final int USER_ID = 0;
    private static final ComponentName TEST_COMPONENT_NAME =
            new ComponentName("test", "test");
    private static final String TEST_PACKAGE_LOCATION = "http://test.location/test.apk";
    private static final byte[] TEST_PACKAGE_CHECKSUM = new byte[]{1};

    private final Context mContext = RuntimeEnvironment.application;
    private final PackageManager mPackageManager = mContext.getPackageManager();
    private final ConnectivityManager mConnectivityManager =
            mContext.getSystemService(ConnectivityManager.class);
    private final AbstractProvisioningTask.Callback mCallback =
            mock(AbstractProvisioningTask.Callback.class);

    @Test
    public void run_doesSetManagedProvisioningDpcDownloaded() {
        final ProvisioningParams params = createDefaultProvisioningParamsBuilder().build();
        final DownloadPackageTask mDownloadPackageTask =
                new DownloadPackageTask(mContext, params, mCallback);

        mDownloadPackageTask.run(USER_ID);

        assertManagedProvisioningDpcDownloadedSetTo(1);
    }

    @Test
    public void run_alreadyInstalled_doesNotSetManagedProvisioningDpcDownloaded() {
        final ProvisioningParams params = createDefaultProvisioningParamsBuilder()
                .setDeviceAdminDownloadInfo(
                        new PackageDownloadInfo.Builder()
                                .setLocation(TEST_PACKAGE_LOCATION)
                                .setMinVersion(1) // installed package is version 2
                                .setPackageChecksum(TEST_PACKAGE_CHECKSUM)
                                .build())
                .build();
        installTestPackage();
        final DownloadPackageTask mDownloadPackageTask =
                new DownloadPackageTask(mContext, params, mCallback);

        mDownloadPackageTask.run(USER_ID);

        assertManagedProvisioningDpcDownloadedSetTo(0);
    }

    @Test
    public void run_installsUpdate_doesSetManagedProvisioningDpcDownloaded() {
        final ProvisioningParams params = createDefaultProvisioningParamsBuilder()
                .setDeviceAdminDownloadInfo(
                        new PackageDownloadInfo.Builder()
                                .setLocation(TEST_PACKAGE_LOCATION)
                                .setMinVersion(3) // installed package is version 2
                                .setPackageChecksum(TEST_PACKAGE_CHECKSUM)
                                .build())
                .build();
        installTestPackage();
        final DownloadPackageTask mDownloadPackageTask =
                new DownloadPackageTask(mContext, params, mCallback);

        mDownloadPackageTask.run(USER_ID);

        assertManagedProvisioningDpcDownloadedSetTo(1);
    }

    @Test
    public void run_notConnected_doesNotSetManagedProvisioningDpcDownloaded() {
        setNotConnected();
        final ProvisioningParams params = createDefaultProvisioningParamsBuilder().build();
        final DownloadPackageTask mDownloadPackageTask =
                new DownloadPackageTask(mContext, params, mCallback);

        mDownloadPackageTask.run(USER_ID);

        assertManagedProvisioningDpcDownloadedSetTo(0);
    }

    private void assertManagedProvisioningDpcDownloadedSetTo(int value) {
        int dpcInstalledSetting =
                Settings.Secure.getInt(
                        mContext.getContentResolver(), MANAGED_PROVISIONING_DPC_DOWNLOADED, 0);
        assertThat(dpcInstalledSetting).isEqualTo(value);
    }

    private void installTestPackage() {
        final PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = TEST_COMPONENT_NAME.getPackageName();
        packageInfo.versionCode = 2;
        shadowOf(mPackageManager).installPackage(packageInfo);
    }

    private void setNotConnected() {
        shadowOf(mConnectivityManager).setActiveNetworkInfo(null);
    }

    private static ProvisioningParams.Builder createDefaultProvisioningParamsBuilder() {
        return new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(TEST_COMPONENT_NAME)
                .setProvisioningAction("")
                .setDeviceAdminDownloadInfo(
                        new PackageDownloadInfo.Builder()
                                .setLocation(TEST_PACKAGE_LOCATION)
                                .setPackageChecksum(TEST_PACKAGE_CHECKSUM)
                                .build());
    }
}
