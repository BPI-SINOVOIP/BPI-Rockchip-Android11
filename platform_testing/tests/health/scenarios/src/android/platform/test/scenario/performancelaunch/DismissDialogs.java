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
package android.platform.test.scenario.performancelaunch;

import android.platform.helpers.HelperAccessor;
import android.platform.helpers.IPerformanceLaunchHelper;
import android.platform.test.rule.NaturalOrientationRule;
import android.platform.test.scenario.annotation.AppSetup;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Dismisses initial dialogs. */
@AppSetup
@Scenario
@RunWith(JUnit4.class)
public class DismissDialogs {
    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    private static HelperAccessor<IPerformanceLaunchHelper> sHelper =
            new HelperAccessor<>(IPerformanceLaunchHelper.class);

    @BeforeClass
    public static void openApp() {
        sHelper.get().open();
    }

    @Test
    public void testDismissDialogs() {
        sHelper.get().dismissInitialDialogs();
    }

    @AfterClass
    public static void closeApp() {
        sHelper.get().exit();
    }
}
