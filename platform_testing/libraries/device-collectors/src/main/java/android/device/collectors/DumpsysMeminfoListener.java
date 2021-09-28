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

package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;

import com.android.helpers.DumpsysMeminfoHelper;

import com.google.common.annotations.VisibleForTesting;

@OptionClass(alias = "dumpsys-meminfo-listener")
public class DumpsysMeminfoListener extends BaseCollectionListener<Long> {

    private static final String TAG = DumpsysMeminfoHelper.class.getSimpleName();
    @VisibleForTesting static final String PROCESS_SEPARATOR = ",";
    @VisibleForTesting static final String PROCESS_NAMES_KEY = "process-names";

    public DumpsysMeminfoListener() {
        createHelperInstance(new DumpsysMeminfoHelper());
    }

    @VisibleForTesting
    public DumpsysMeminfoListener(Bundle args, DumpsysMeminfoHelper helper) {
        super(args, helper);
    }

    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        String processesString = args.getString(PROCESS_NAMES_KEY);
        if (processesString == null) {
            Log.w(TAG, "No process name provided. Nothing will be collected");
            return;
        }
        ((DumpsysMeminfoHelper) mHelper).setUp(processesString.split(PROCESS_SEPARATOR));
    }
}
