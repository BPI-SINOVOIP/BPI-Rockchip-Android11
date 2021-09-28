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

package com.android.server.wm.flicker;

import static com.android.server.wm.flicker.CommonTransitions.openAppCold;
import static com.android.server.wm.flicker.WmTraceSubject.assertThat;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.junit.runners.Parameterized;

/**
 * Test cold launch app from launcher.
 * To run this test: {@code atest FlickerTests:OpenAppColdTest}
 */
@LargeTest
@RunWith(Parameterized.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class OpenAppColdTest extends NonRotationTestBase {

    public OpenAppColdTest(String beginRotationName, int beginRotation) {
        super(beginRotationName, beginRotation);

        this.mTestApp = new StandardAppHelper(InstrumentationRegistry.getInstrumentation(),
                "com.android.server.wm.flicker.testapp", "SimpleApp");
    }

    @Before
    public void runTransition() {
        run(openAppCold(mTestApp, mUiDevice, mBeginRotation)
                .includeJankyRuns().build());
    }

    @Test
    public void checkVisibility_wallpaperWindowBecomesInvisible() {
        checkResults(result -> assertThat(result)
                .showsBelowAppWindow("Wallpaper")
                .then()
                .hidesBelowAppWindow("Wallpaper")
                .forAllEntries());
    }

    @FlakyTest(bugId = 140855415)
    @Ignore("Waiting bug feedback")
    @Test
    public void checkZOrder_appWindowReplacesLauncherAsTopWindow() {
        checkResults(result -> assertThat(result)
                .showsAppWindowOnTop(
                        "com.android.launcher3/.Launcher")
                .then()
                .showsAppWindowOnTop(mTestApp.getPackage())
                .forAllEntries());
    }

    @Test
    public void checkVisibility_wallpaperLayerBecomesInvisible() {
        checkResults(result -> LayersTraceSubject.assertThat(result)
                .showsLayer("Wallpaper")
                .then()
                .hidesLayer("Wallpaper")
                .forAllEntries());
    }
}
