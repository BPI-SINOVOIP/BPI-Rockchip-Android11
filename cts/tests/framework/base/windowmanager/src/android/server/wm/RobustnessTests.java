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
 * limitations under the License
 */

package android.server.wm;

import static android.server.wm.app.Components.RECURSIVE_ACTIVITY;
import static android.server.wm.app.Components.TEST_ACTIVITY;

import android.platform.test.annotations.Presubmit;

import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:RobustnessTests
 */
@Presubmit
public class RobustnessTests extends ActivityManagerTestBase {

    /**
     * Tests launching an activity that launches itself recursively limited number of times.
     * This is a long test, so shouldn't be part of presubmit.
     */
    @Test
    public void testLaunchActivityRecursively() throws Exception {
        getLaunchActivityBuilder().setUseInstrumentation()
                .setTargetActivity(RECURSIVE_ACTIVITY).execute();

        for (int retry = 1; retry <= 10; retry++) {
            mWmState.computeState(TEST_ACTIVITY);
            if (mWmState.isActivityVisible(TEST_ACTIVITY)) {
                break;
            }
        }
        mWmState.waitForValidState(TEST_ACTIVITY);
        mWmState.assertVisibility(TEST_ACTIVITY, true);
    }
}
