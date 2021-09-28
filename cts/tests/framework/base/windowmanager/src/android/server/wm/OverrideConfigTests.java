/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.server.wm;

import static android.app.WindowConfiguration.WINDOWING_MODE_FREEFORM;
import static android.server.wm.app.Components.LOG_CONFIGURATION_ACTIVITY;
import static android.view.Surface.ROTATION_0;
import static android.view.Surface.ROTATION_180;

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.Presubmit;
import android.server.wm.CommandSession.ActivityCallback;

import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:OverrideConfigTests
 */
@Presubmit
public class OverrideConfigTests extends ActivityManagerTestBase {

    @Test
    public void testReceiveOverrideConfigFromRelayout() {
        assumeTrue("Device doesn't support freeform. Skipping test.", supportsFreeform());

        launchActivity(LOG_CONFIGURATION_ACTIVITY, WINDOWING_MODE_FREEFORM);

        final RotationSession rotationSession = createManagedRotationSession();
        rotationSession.set(ROTATION_0);
        separateTestJournal();
        resizeActivityTask(LOG_CONFIGURATION_ACTIVITY, 0, 0, 100, 100);
        new ActivityLifecycleCounts(LOG_CONFIGURATION_ACTIVITY).assertCountWithRetry(
                "Expected to observe configuration change when resizing",
                countSpec(ActivityCallback.ON_CONFIGURATION_CHANGED, CountSpec.EQUALS, 1));

        separateTestJournal();
        rotationSession.set(ROTATION_180);
        new ActivityLifecycleCounts(LOG_CONFIGURATION_ACTIVITY).assertCountWithRetry(
                "Not expected to observe configuration change after flip rotation",
                countSpec(ActivityCallback.ON_CONFIGURATION_CHANGED, CountSpec.EQUALS, 0));
    }
}

