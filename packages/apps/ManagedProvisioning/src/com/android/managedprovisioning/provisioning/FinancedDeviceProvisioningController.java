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

import android.content.Context;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.model.ProvisioningParams;
import com.android.managedprovisioning.task.AbstractProvisioningTask;
import com.android.managedprovisioning.task.DeviceOwnerInitializeProvisioningTask;
import com.android.managedprovisioning.task.DisallowAddUserTask;
import com.android.managedprovisioning.task.DownloadPackageTask;
import com.android.managedprovisioning.task.InstallPackageTask;
import com.android.managedprovisioning.task.SetDevicePolicyTask;
import com.android.managedprovisioning.task.VerifyPackageTask;

/**
 * Controller for financed device provisioning.
 */
public final class FinancedDeviceProvisioningController extends AbstractProvisioningController  {

    public FinancedDeviceProvisioningController(
            Context context,
            ProvisioningParams params,
            int userId,
            ProvisioningControllerCallback callback) {
        super(context, params, userId, callback);
    }

    @Override
    protected void setUpTasks() {
        addTasks(new DeviceOwnerInitializeProvisioningTask(mContext, mParams, this));

        if (mParams.deviceAdminDownloadInfo != null) {
            DownloadPackageTask downloadTask = new DownloadPackageTask(mContext, mParams, this);
            addTasks(downloadTask,
                    new VerifyPackageTask(downloadTask, mContext, mParams, this),
                    new InstallPackageTask(downloadTask, mContext, mParams, this));
        }
        addTasks(
                new SetDevicePolicyTask(mContext, mParams, this),
                new DisallowAddUserTask(mContext, mParams, this));
    }

    @Override
    protected void performCleanup() {
        // do nothing here because factory reset will be enforced
    }

    @Override
    protected int getErrorTitle() {
        return R.string.something_went_wrong;
    }

    @Override
    protected int getErrorMsgId(AbstractProvisioningTask task, int errorCode) {
        return R.string.reset_device;
    }

    @Override
    protected boolean getRequireFactoryReset(AbstractProvisioningTask task, int errorCode) {
        return true;
    }
}
