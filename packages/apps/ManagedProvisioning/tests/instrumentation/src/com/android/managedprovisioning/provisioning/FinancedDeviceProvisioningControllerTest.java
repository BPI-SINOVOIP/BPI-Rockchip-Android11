/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.provisioning;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_FINANCED_DEVICE;

import static org.mockito.Mockito.verify;

import android.content.ComponentName;

import androidx.test.filters.SmallTest;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.model.PackageDownloadInfo;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.task.DeviceOwnerInitializeProvisioningTask;
import com.android.managedprovisioning.task.DisallowAddUserTask;
import com.android.managedprovisioning.task.DownloadPackageTask;
import com.android.managedprovisioning.task.InstallPackageTask;
import com.android.managedprovisioning.task.SetDevicePolicyTask;
import com.android.managedprovisioning.task.VerifyPackageTask;
import org.mockito.Mock;

/**
 * Unit tests for {@link FinancedDeviceProvisioningController}.
 */
public class FinancedDeviceProvisioningControllerTest extends ProvisioningControllerBaseTest {

    private static final int TEST_USER_ID = 123;

    private static final ComponentName TEST_ADMIN = new ComponentName("com.test.admin",
        "com.test.admin.AdminReceiver");

    private static final String TEST_DOWNLOAD_LOCATION = "http://www.some.uri.com";
    private static final byte[] TEST_PACKAGE_CHECKSUM = new byte[] { '1', '2', '3', '4', '5' };
    private static final PackageDownloadInfo TEST_DOWNLOAD_INFO = new PackageDownloadInfo.Builder()
            .setLocation(TEST_DOWNLOAD_LOCATION)
            .setSignatureChecksum(TEST_PACKAGE_CHECKSUM)
            .build();

    @Mock private ProvisioningControllerCallback mCallback;

    @SmallTest
    public void testRunAllTasks() throws Exception {
        // GIVEN financed device provisioning was invoked
        createController(createProvisioningParamsBuilder().build());

        // WHEN starting the test run
        mController.start(mHandler);

        // THEN the initialization task is run first
        taskSucceeded(DeviceOwnerInitializeProvisioningTask.class);

        // THEN the download package task should be run
        taskSucceeded(DownloadPackageTask.class);

        // THEN the verify package task should be run
        taskSucceeded(VerifyPackageTask.class);

        // THEN the install package task should be run
        taskSucceeded(InstallPackageTask.class);

        // THEN the set device policy task should be run
        taskSucceeded(SetDevicePolicyTask.class);

        // THEN the disallow add user task should be run
        taskSucceeded(DisallowAddUserTask.class);

        // THEN the provisioning complete callback should have happened
        verify(mCallback).provisioningTasksCompleted();
    }

    @SmallTest
    public void testNoDownloadInfo() throws Exception {
        // GIVEN financed device provisioning was invoked with no download info
        createController(createProvisioningParamsBuilder()
                .setDeviceAdminDownloadInfo(null)
                .build());

        // WHEN starting the test run
        mController.start(mHandler);

        // THEN the initialization task is run first
        taskSucceeded(DeviceOwnerInitializeProvisioningTask.class);

        // THEN the set device policy task should be run
        taskSucceeded(SetDevicePolicyTask.class);

        // THEN the disallow add user task should be run
        taskSucceeded(DisallowAddUserTask.class);

        // THEN the provisioning complete callback should have happened
        verify(mCallback).provisioningTasksCompleted();
    }

    private void createController(ProvisioningParams params) {
        mController = new FinancedDeviceProvisioningController(
                getContext(),
                params,
                TEST_USER_ID,
                mCallback);
    }

    private ProvisioningParams.Builder createProvisioningParamsBuilder() {
        return new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(TEST_ADMIN)
                .setDeviceAdminDownloadInfo(TEST_DOWNLOAD_INFO)
                .setProvisioningAction(ACTION_PROVISION_FINANCED_DEVICE);
    }
}
