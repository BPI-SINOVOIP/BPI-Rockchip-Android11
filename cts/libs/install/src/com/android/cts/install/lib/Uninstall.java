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

package com.android.cts.install.lib;

import android.content.Context;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;

import androidx.test.InstrumentationRegistry;

/**
 * Helper class for uninstalling test apps.
 */
public class Uninstall {
    /**
     * Uninstalls all of the given packages.
     * Does nothing if the package is not installed or installed on /system
     */
    public static void packages(String... packageNames) throws InterruptedException {
        for (String pkg : packageNames) {
            uninstallSinglePackage(pkg);
        }
    }

    private static void uninstallSinglePackage(String packageName) throws InterruptedException {
        // No need to uninstall if the package isn't installed.
        if (InstallUtils.getInstalledVersion(packageName) == -1
                || InstallUtils.isSystemAppWithoutUpdate(packageName)) {
            return;
        }

        Context context = InstrumentationRegistry.getContext();
        PackageManager packageManager = context.getPackageManager();
        PackageInstaller packageInstaller = packageManager.getPackageInstaller();
        packageInstaller.uninstall(packageName, LocalIntentSender.getIntentSender());
        InstallUtils.assertStatusSuccess(LocalIntentSender.getIntentSenderResult());
    }
}
