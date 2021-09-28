/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.tests;

import android.content.pm.PackageManager;

import com.android.game.qualification.GameCoreConfiguration;

import static org.junit.Assume.assumeTrue;

public class TestUtils {
    public static boolean isGameCoreCertified(PackageManager pm) {
        return pm.hasSystemFeature(GameCoreConfiguration.FEATURE_STRING);
    }

    public static void assumeGameCoreCertified(PackageManager pm) {
        assumeTrue(isGameCoreCertified(pm));
    }
}
