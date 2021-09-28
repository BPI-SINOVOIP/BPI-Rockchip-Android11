/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.compatibility.common.util;

import android.util.Log;

/**
 * Device-side utility class for override/reset thermal status.
 */
public final class ThermalUtils {
    private static final String TAG = "CtsThermalUtils";

    private ThermalUtils() {}

    /** Make the target device think it's not throttling. */
    public static void overrideThermalNotThrottling() throws Exception {
        overrideThermalStatus(0);
    }

    /**
     * Make the target device think it's in given throttling status.
     * @param status thermal status defined in android.os.Temperature
     */
    public static void overrideThermalStatus(int status) throws Exception {
        SystemUtil.runShellCommandForNoOutput("cmd thermalservice override-status " + status);

        Log.d(TAG, "override-status " + status);
    }

    /** Cancel the thermal override status on target device. */
    public static void resetThermalStatus() throws Exception {
        SystemUtil.runShellCommandForNoOutput("cmd thermalservice reset");

        Log.d(TAG, "reset");
    }
}
