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

package com.android.cts.verifier.tv;

import android.util.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

public class TvUtil {
    public static final String LOG_TAG = "TvUtil";

    public static boolean isHdmiSourceDevice() {
        final int DEVICE_TYPE_HDMI_SOURCE = 4;
        try {
            return getHdmiDeviceType().contains(DEVICE_TYPE_HDMI_SOURCE);
        } catch (Exception exception) {
            Log.e(LOG_TAG, "Exception while looking up HDMI device type.", exception);
        }
        return false;
    }

    public static List<Integer> getHdmiDeviceType()
            throws InvocationTargetException, IllegalAccessException, ClassNotFoundException,
            NoSuchMethodException {
        Method getStringMethod =
                ClassLoader.getSystemClassLoader()
                        .loadClass("android.os.SystemProperties")
                        .getMethod("get", String.class);
        String deviceTypesStr = (String) getStringMethod.invoke(null, "ro.hdmi.device_type");
        if (deviceTypesStr.equals("")) {
            return new ArrayList<>();
        }
        return Arrays.stream(deviceTypesStr.split(","))
                .map(Integer::parseInt)
                .collect(Collectors.toList());
    }
}
