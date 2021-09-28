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

package com.android.framebufferizer.utils;


public class DeviceInfoDB {
    public static enum Device { BLUELINE, BONITO, CROSSHATCH, CORAL, SARGO};
    public static final double DEFAULT_DENSITY_INDEPENDENT_WIDTH = 412.0;

    static DeviceInfo getDeviceInfo(Device device) {
        switch (device) {
            case BLUELINE:
                return new DeviceInfo(1080, 2160, 1080/DEFAULT_DENSITY_INDEPENDENT_WIDTH, 17.42075974, 20.26, 30.26, 40.26, 50.26);
            case CROSSHATCH:
                return new DeviceInfo(1440, 2950, 1440/DEFAULT_DENSITY_INDEPENDENT_WIDTH, 20.42958729, 34.146, 44.146, 54.146, 64.146);
            case BONITO:
            case CORAL:
            case SARGO:
                // return Blueline for now. TODO needs correnct values for other devices
                return new DeviceInfo(1080, 2160, 1080/DEFAULT_DENSITY_INDEPENDENT_WIDTH, 17.42075974, 20.26, 30.26, 40.26, 50.26);
        }
        return new DeviceInfo(1080, 2160, 1080/DEFAULT_DENSITY_INDEPENDENT_WIDTH, 17.42075974, 20.26, 30.26, 40.26, 50.26);
    }
}
