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

package com.android.cts.appdataisolation.appa;

import static com.android.cts.appdataisolation.common.FileUtils.assertDirDoesNotExist;
import static com.android.cts.appdataisolation.common.FileUtils.assertDirIsNotAccessible;

import android.app.Service;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.IBinder;
import android.os.RemoteException;

import com.android.cts.appdataisolation.common.FileUtils;

public class IsolatedService extends Service {

    private final IIsolatedService.Stub mBinder = new IIsolatedService.Stub() {

        // Test cannot access its data directory, and also can't see other apps data dir.
        public void assertDataIsolated() throws RemoteException {
            try {
                ApplicationInfo applicationInfo = getApplicationInfo();
                assertDirDoesNotExist(applicationInfo.dataDir);
                assertDirDoesNotExist(applicationInfo.deviceProtectedDataDir);
                assertDirDoesNotExist("/data/data/" + getPackageName());
                assertDirDoesNotExist("/data/misc/profiles/cur/0/" + getPackageName());
                assertDirIsNotAccessible("/data/misc/profiles/ref");

                assertDirDoesNotExist(FileUtils.replacePackageAWithPackageB(
                        applicationInfo.dataDir));
                assertDirDoesNotExist(FileUtils.replacePackageAWithPackageB(
                        applicationInfo.deviceProtectedDataDir));
                assertDirDoesNotExist("/data/data/" + FileUtils.APPB_PKG);
                assertDirDoesNotExist("/data/misc/profiles/cur/0/" + FileUtils.APPB_PKG);

                assertDirDoesNotExist(FileUtils.replacePackageAWithNotInstalledPkg(
                        applicationInfo.dataDir));
                assertDirDoesNotExist(FileUtils.replacePackageAWithNotInstalledPkg(
                        applicationInfo.deviceProtectedDataDir));
                assertDirDoesNotExist("/data/data/" + FileUtils.NOT_INSTALLED_PKG);
                assertDirDoesNotExist("/data/misc/profiles/cur/0/" + FileUtils.NOT_INSTALLED_PKG);
            } catch (Throwable e) {
                throw new IllegalStateException(e.getMessage());
            }
        }

    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
