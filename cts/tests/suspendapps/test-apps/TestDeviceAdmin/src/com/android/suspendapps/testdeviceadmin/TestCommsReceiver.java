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

package com.android.suspendapps.testdeviceadmin;

import static android.app.Activity.RESULT_OK;

import android.app.admin.DevicePolicyManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.util.Arrays;

public class TestCommsReceiver extends BroadcastReceiver {
    private static final String TAG = TestCommsReceiver.class.getSimpleName();

    public static final String PACKAGE_NAME = "com.android.suspendapps.testdeviceadmin";
    public static final String ACTION_UNSUSPEND = PACKAGE_NAME + ".action.UNSUSPEND";
    public static final String ACTION_SUSPEND = PACKAGE_NAME + ".action.SUSPEND";
    public static final String ACTION_BLOCK_UNINSTALL = PACKAGE_NAME + ".action.BLOCK_UNINSTALL";
    public static final String ACTION_UNBLOCK_UNINSTALL =
            PACKAGE_NAME + ".action.UNBLOCK_UNINSTALL";
    public static final String ACTION_ADD_USER_RESTRICTION =
            PACKAGE_NAME + ".action.ADD_USER_RESTRICTION";
    public static final String ACTION_CLEAR_USER_RESTRICTION =
            PACKAGE_NAME + ".action.CLEAR_USER_RESTRICTION";
    public static final String EXTRA_USER_RESTRICTION = PACKAGE_NAME + ".extra.USER_RESTRICTION";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d(TAG, "Received request " + intent.getAction());

        final DevicePolicyManager dpm = context.getSystemService(DevicePolicyManager.class);
        final ComponentName deviceAdmin = new ComponentName(PACKAGE_NAME,
                TestDeviceAdmin.class.getName());
        final String packageName = intent.getStringExtra(Intent.EXTRA_PACKAGE_NAME);
        final String userRestriction = intent.getStringExtra(EXTRA_USER_RESTRICTION);
        boolean suspend = false;
        boolean block = false;
        switch (intent.getAction()) {
            case ACTION_SUSPEND:
                suspend = true;
                // Intentional fall-through
            case ACTION_UNSUSPEND:
                if (packageName == null) {
                    Log.e(TAG, "Need package to complete suspend/unsuspend request");
                    break;
                }
                final String[] errored = dpm.setPackagesSuspended(deviceAdmin,
                        new String[]{packageName}, suspend);
                if (errored.length > 0) {
                    Log.e(TAG, "Could not update packages: " + Arrays.toString(errored)
                            + " for request: " + intent.getAction());
                    break;
                }
                setResultCode(RESULT_OK);
                break;
            case ACTION_BLOCK_UNINSTALL:
                block = true;
                // Intentional fall-through
            case ACTION_UNBLOCK_UNINSTALL:
                if (packageName == null) {
                    Log.e(TAG, "Need package to complete block/unblock uninstall request");
                    break;
                }
                dpm.setUninstallBlocked(deviceAdmin, packageName, block);
                setResultCode(RESULT_OK);
                break;
            case ACTION_ADD_USER_RESTRICTION:
                if (userRestriction == null) {
                    Log.e(TAG, "No user restriction provided to set");
                    break;
                }
                dpm.addUserRestriction(deviceAdmin, userRestriction);
                setResultCode(RESULT_OK);
                break;
            case ACTION_CLEAR_USER_RESTRICTION:
                if (userRestriction == null) {
                    Log.e(TAG, "No user restriction provided to clear");
                    break;
                }
                dpm.clearUserRestriction(deviceAdmin, userRestriction);
                setResultCode(RESULT_OK);
                break;
        }
    }
}
