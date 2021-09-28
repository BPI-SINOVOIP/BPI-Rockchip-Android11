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
package android.platform.test.scenario.performancelaunch.hermeticapp;

import android.platform.test.rule.NaturalOrientationRule;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Opens the Performance Launch Empty WebView activity and exits after. */
@Scenario
@RunWith(JUnit4.class)
public class OpenEmptyWebViewActivity extends PerformanceBase {

    private static final String EMPTY_WEB_VIEW_ACTIVITY =
            "com.android.performanceLaunch.EmptyWebViewActivity";

    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    @Test
    public void testOpen() {
        open(EMPTY_WEB_VIEW_ACTIVITY);
    }
}
