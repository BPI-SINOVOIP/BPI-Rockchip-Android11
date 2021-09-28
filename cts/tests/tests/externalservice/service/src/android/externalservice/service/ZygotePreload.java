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
 * limitations under the License.
 */

package android.externalservice.service;

import android.content.pm.ApplicationInfo;
import android.os.Process;
import android.util.Log;

public class ZygotePreload implements android.app.ZygotePreload {
    private static final String TAG = "ZygotePreload";

    private static int sZygotePid = -1;

    private static String sZygotePackage = null;

    public static int getZygotePid() {
        return sZygotePid;
    }

    public static String getZygotePackage() {
        return sZygotePackage;
    }

    public void doPreload(ApplicationInfo info) {
        sZygotePid = Process.myPid();
        sZygotePackage = info.packageName;
        Log.d(TAG, "Doing ZygotePreload, sZygotePid=" + sZygotePid);
    }
}
