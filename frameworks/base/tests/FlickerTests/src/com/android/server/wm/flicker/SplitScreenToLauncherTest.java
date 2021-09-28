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

import static com.android.server.wm.flicker.CommonTransitions.splitScreenToLauncher;
import static com.android.server.wm.flicker.WindowUtils.getDisplayBounds;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

/**
 * Test open app to split screen.
 * To run this test: {@code atest FlickerTests:SplitScreenToLauncherTest}
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
@FlakyTest(bugId = 140856143)
@Ignore("Waiting bug feedback")
public class SplitScreenToLauncherTest extends FlickerTestBase {

    public SplitScreenToLauncherTest() {
        this.mTestApp = new StandardAppHelper(InstrumentationRegistry.getInstrumentation(),
                "com.android.server.wm.flicker.testapp", "SimpleApp");
    }

    @Before
    public void runTransition() {
        super.runTransition(splitScreenToLauncher(mTestApp, mUiDevice).includeJankyRuns().build());
    }

    @Test
    public void checkCoveredRegion_noUncoveredRegions() {
        checkResults(result ->
                LayersTraceSubject.assertThat(result)
                        .coversRegion(getDisplayBounds()).forAllEntries());
    }

    @Test
    public void checkVisibility_dividerLayerBecomesInVisible() {
        checkResults(result -> LayersTraceSubject.assertThat(result)
                .showsLayer(DOCKED_STACK_DIVIDER)
                .then()
                .hidesLayer(DOCKED_STACK_DIVIDER)
                .forAllEntries());
    }

    @Test
    public void checkVisibility_appLayerBecomesInVisible() {
        checkResults(result -> LayersTraceSubject.assertThat(result)
                .showsLayer(mTestApp.getPackage())
                .then()
                .hidesLayer(mTestApp.getPackage())
                .forAllEntries());
    }

    @Test
    public void checkVisibility_navBarWindowIsAlwaysVisible() {
        checkResults(result -> WmTraceSubject.assertThat(result)
                .showsAboveAppWindow(NAVIGATION_BAR_WINDOW_TITLE).forAllEntries());
    }

    @Test
    public void checkVisibility_statusBarWindowIsAlwaysVisible() {
        checkResults(result -> WmTraceSubject.assertThat(result)
                .showsAboveAppWindow(STATUS_BAR_WINDOW_TITLE).forAllEntries());
    }

    @Test
    public void checkVisibility_dividerWindowBecomesInVisible() {
        checkResults(result -> WmTraceSubject.assertThat(result)
                .showsAboveAppWindow(DOCKED_STACK_DIVIDER)
                .then()
                .hidesAboveAppWindow(DOCKED_STACK_DIVIDER)
                .forAllEntries());
    }
}
