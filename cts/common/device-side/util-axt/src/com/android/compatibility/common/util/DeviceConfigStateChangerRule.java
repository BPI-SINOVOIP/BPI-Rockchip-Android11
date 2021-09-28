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
package com.android.compatibility.common.util;

import android.content.Context;
import android.provider.DeviceConfig;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * JUnit rule used to set a {@link DeviceConfig} preference before the test is run.
 *
 * <p>It stores the current value before the test, changes it (if necessary), then restores it after
 * the test (if necessary).
 */
public class DeviceConfigStateChangerRule extends StateChangerRule<String> {

    /**
     * Default constructor.
     *
     * @param context context used to retrieve the {@link DeviceConfig} provider.
     * @param namespace {@code DeviceConfig} namespace.
     * @param key prefence key.
     * @param value value to be set before the test is run.
     */
    public DeviceConfigStateChangerRule(@NonNull Context context, @NonNull String namespace,
            @NonNull String key, @Nullable String value) {
        this(new DeviceConfigStateManager(context, namespace, key), value);
    }

    /**
     * Alternative constructor used when then test case already defines a
     * {@link DeviceConfigStateManager}.
     */
    public DeviceConfigStateChangerRule(@NonNull DeviceConfigStateManager dcsm,
            @Nullable String value) {
        super(dcsm, value);
    }
}
