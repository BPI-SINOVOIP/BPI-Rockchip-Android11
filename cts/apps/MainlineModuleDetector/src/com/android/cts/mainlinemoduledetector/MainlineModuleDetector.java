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
package com.android.cts.mainlinemoduledetector;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

import com.android.compatibility.common.util.mainline.MainlineModule;
import com.android.compatibility.common.util.mainline.ModuleDetector;

import java.util.HashSet;
import java.util.Set;

public class MainlineModuleDetector extends Activity {

    private static final String LOG_TAG = "MainlineModuleDetector";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            PackageManager pm = getApplicationContext().getPackageManager();
            Set<MainlineModule> modules = ModuleDetector.getPlayManagedModules(pm);
            Set<String> moduleNames = new HashSet<>();
            for (MainlineModule module : modules) {
                moduleNames.add(module.packageName);
            }
            Log.i(LOG_TAG, "Play managed modules are: <" + String.join(",", moduleNames) + ">");
        } catch (Exception e) {
            Log.e(LOG_TAG, "Failed to retrieve modules.", e);
        }
        this.finish();
    }
}
