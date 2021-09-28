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

package com.android.tradefed.device.battery;

import com.android.ddmlib.IDevice;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.StreamUtil;

import com.google.common.collect.ImmutableSet;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.Set;

/** Utility class that allows to control the battery charging state of a device. */
public class BatteryController {

    /** List of supported device */
    private static Map<String, IBatteryInfo> sSupportProduct = new HashMap<>();

    // TODO: Allow loading from an external file
    private static final Set<String> BATTERY_CONFIGS =
            ImmutableSet.of("/battery_config/battery.cfg", "/battery_config/google_battery.cfg");
    private static boolean sConfigLoaded = false;

    /**
     * Returns the charging state of the device. If device is not support it will return {@link
     * IBatteryInfo.BatteryState#UNDEFINED}.
     */
    public static IBatteryInfo.BatteryState getDeviceChargingState(ITestDevice device)
            throws DeviceNotAvailableException {
        // Load the configurations if needed.
        if (!sConfigLoaded) {
            loadConfigs();
        }

        IDevice idevice = device.getIDevice();
        if (idevice instanceof StubDevice) {
            return IBatteryInfo.BatteryState.UNDEFINED;
        }
        String product = device.getProductType();
        IBatteryInfo info = sSupportProduct.get(product);
        if (info == null) {
            CLog.d(
                    "Device product %s is not supported. Check battery_config/battery.csv.",
                    product);
            return IBatteryInfo.BatteryState.UNDEFINED;
        }
        if (!device.isAdbRoot()) {
            return IBatteryInfo.BatteryState.INFEASIBLE;
        }
        return info.checkBatteryState(device);
    }

    /**
     * Returns the {@link IBatteryInfo} of a device. Returns null if something goes wrong or if the
     * device is not supported.
     */
    public static IBatteryInfo getBatteryInfoForDevice(ITestDevice device) {
        // Load the configurations if needed.
        if (!sConfigLoaded) {
            loadConfigs();
        }

        String product = null;
        try {
            product = device.getProductType();
            return sSupportProduct.get(product);
        } catch (DeviceNotAvailableException e) {
            CLog.e("Failed to get the device product type for getBatteryInfoForDevice.");
        }
        return null;
    }

    private static void loadConfigs() {
        synchronized (BatteryController.class) {
            for (String resourceConfig : BATTERY_CONFIGS) {
                BufferedReader reader = null;
                try (InputStream resStream =
                        BatteryController.class.getResourceAsStream(resourceConfig)) {
                    if (resStream == null) {
                        CLog.w("No battery configuration file for resource: %s", resourceConfig);
                        continue;
                    }
                    Properties properties = new Properties();
                    properties.load(resStream);
                    for (Object key : properties.keySet()) {
                        String deviceName = (String) key;
                        String batteryClass = (String) properties.get(key);
                        try {
                            Object battery =
                                    Class.forName(batteryClass)
                                            .getDeclaredConstructor()
                                            .newInstance();
                            if (battery instanceof IBatteryInfo) {
                                sSupportProduct.put(deviceName, (IBatteryInfo) battery);
                            }
                        } catch (InstantiationException
                                | IllegalAccessException
                                | ClassNotFoundException
                                | InvocationTargetException
                                | NoSuchMethodException e) {
                            CLog.e(e);
                        }
                    }
                } catch (IOException e) {
                    CLog.e(e);
                } finally {
                    StreamUtil.close(reader);
                }
            }
            sConfigLoaded = true;
        }
    }
}
