/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.module;

import android.content.Context;

import com.android.wallpaper.config.Flags;

/**
 * Default implementation of {@code FormFactorChecker} which inspects the device to determine its
 * form factor.
 */
public class DefaultFormFactorChecker implements FormFactorChecker {
    private Context mAppContext;

    public DefaultFormFactorChecker(Context appContext) {
        mAppContext = appContext;
    }

    @FormFactor
    @Override
    public int getFormFactor() {
        if (!Flags.desktopUiEnabled) {
            return FORM_FACTOR_MOBILE;
        }

        return mAppContext.getPackageManager().hasSystemFeature(
                "org.chromium.arc.device_management") ? FORM_FACTOR_DESKTOP : FORM_FACTOR_MOBILE;
    }
}
