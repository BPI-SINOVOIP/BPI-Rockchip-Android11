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

package com.android.server.telecom;

import android.annotation.NonNull;
import android.content.Context;
import android.os.PowerWhitelistManager;
import android.os.RemoteException;
import android.telecom.Log;

import com.android.internal.telecom.IDeviceIdleControllerAdapter;

/**
 * Telecom is in the same process as the {@link PowerWhitelistManager}, so we can not make direct
 * calls to the manager interface, since they will fail in the DeviceIdleController
 * (see {@link Context#enforceCallingPermission(String, String)}). Instead, we must access it
 * through SystemService#getLocalService, which is only accessible to the Telecom
 * core loader service (TelecomLoaderService). Unfortunately, due to the architecture, this means
 * we must use a Binder to allow services such as this to be accessible.
 */
public class DeviceIdleControllerAdapter {

    private static final String TAG = "DeviceIdleControllerAdapter";

    private final IDeviceIdleControllerAdapter mAdapter;

    public DeviceIdleControllerAdapter(IDeviceIdleControllerAdapter adapter) {
        mAdapter = adapter;
    }

    /**
     * Exempts an application from power restrictions for the duration specified. See
     * DeviceIdleController for more information on how this works.
     */
    public void exemptAppTemporarilyForEvent(@NonNull String packageName, long duration,
            int userHandle, @NonNull String reason) {
        try {
            mAdapter.exemptAppTemporarilyForEvent(packageName, duration, userHandle, reason);
        } catch (RemoteException e) {
            Log.w(TAG, "exemptAppTemporarilyForEvent e=" + e.getMessage());
        }
    }
}
