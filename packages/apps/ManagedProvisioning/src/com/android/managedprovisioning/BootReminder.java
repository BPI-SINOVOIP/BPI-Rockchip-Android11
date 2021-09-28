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
package com.android.managedprovisioning;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.android.managedprovisioning.ota.CrossProfileAppsPregrantController;
import com.android.managedprovisioning.preprovisioning.EncryptionController;

/**
 * Boot listener for triggering reminders at boot time.
 */
public class BootReminder extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        new CrossProfileAppsPregrantController(context).checkCrossProfileAppsPermissions();

        // For encryption flows during setup wizard, this acts as a backup to
        // PostEncryptionActivity in case the PackageManager has not yet written the package state
        // to disk when the reboot is triggered.
        EncryptionController.getInstance(context).resumeProvisioning();
    }
}

