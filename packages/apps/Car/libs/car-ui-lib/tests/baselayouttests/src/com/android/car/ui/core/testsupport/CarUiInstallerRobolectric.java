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
package com.android.car.ui.core.testsupport;

import android.content.pm.ProviderInfo;

import com.android.car.ui.core.CarUiInstaller;

import org.robolectric.Robolectric;

/**
 * A helper class that can be used to install the base layout into robolectric test activities.
 */
public class CarUiInstallerRobolectric {

    public static void install() {
        ProviderInfo info = new ProviderInfo();
        Class carUiInstallerClass = CarUiInstaller.class;
        info.authority = carUiInstallerClass.getName();
        Robolectric.buildContentProvider(carUiInstallerClass).create(info);
    }
}
