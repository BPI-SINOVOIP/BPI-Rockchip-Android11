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
package android.car.test.util;

import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyAccess;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyChangeMode;

/**
 * Provides utilities for Vehicle HAL related tasks.
 */
public final class VehicleHalTestingHelper {

    /**
     * Creates an empty config for the given property.
     */
    public static VehiclePropConfig newConfig(int prop) {
        VehiclePropConfig config = new VehiclePropConfig();
        config.prop = prop;
        return config;
    }

    /**
     * Creates a config for the given property that passes the
     * {@link com.android.car.hal.VehicleHal.VehicleHal#isPropertySubscribable(VehiclePropConfig)}
     * criteria.
     */
    public static VehiclePropConfig newSubscribableConfig(int prop) {
        VehiclePropConfig config = newConfig(prop);
        config.access = VehiclePropertyAccess.READ_WRITE;
        config.changeMode = VehiclePropertyChangeMode.ON_CHANGE;
        return config;
    }

    private VehicleHalTestingHelper() {
        throw new UnsupportedOperationException("contains only static methods");
    }
}
