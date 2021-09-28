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

package com.android.uibench.microbenchmark;

import android.platform.helpers.HelperAccessor;
import android.platform.test.microbenchmark.Microbenchmark;
import android.platform.test.rule.NaturalOrientationRule;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(Microbenchmark.class)
public class UiBenchLayoutCacheLowHitrateFlingMicrobenchmark {
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    private static HelperAccessor<IUiBenchJankHelper> sHelper =
            new HelperAccessor<>(IUiBenchJankHelper.class);

    @BeforeClass
    public static void openApp() {
        sHelper.get().openLayoutCacheLowHitrate();
    }

    @Test
    public void testLayoutCacheLowHitrateFling() {
        sHelper.get().flingUpDown(3);
    }

    @AfterClass
    public static void closeApp() {
        sHelper.get().exit();
    }
}
