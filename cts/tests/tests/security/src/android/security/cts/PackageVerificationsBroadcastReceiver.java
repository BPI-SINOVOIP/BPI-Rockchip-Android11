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

package android.security.cts;

import static android.content.pm.PackageManager.EXTRA_VERIFICATION_PACKAGE_NAME;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public final class PackageVerificationsBroadcastReceiver extends BroadcastReceiver {

    private static final String TAG = "PackageInstallerTest";

    static final BlockingQueue<String> packages = new LinkedBlockingQueue<>();

    @Override
    public void onReceive(Context context, Intent intent) {
        final String packageName = intent.getStringExtra(EXTRA_VERIFICATION_PACKAGE_NAME);
        Log.i(TAG, "Received PACKAGE_NEEDS_VERIFICATION broadcast for package " + packageName);
        try {
            packages.put(packageName);
        } catch (InterruptedException e) {
            throw new AssertionError(e);
        }
    }
}
