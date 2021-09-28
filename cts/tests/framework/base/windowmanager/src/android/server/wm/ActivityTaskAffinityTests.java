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

package android.server.wm;

import static android.server.wm.second.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY_DIFFERENT_UID;
import static android.server.wm.shareuid.a.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY;
import static android.server.wm.shareuid.a.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY_SAME_APP;
import static android.server.wm.shareuid.b.Components.TEST_ACTIVITY_WITH_SAME_AFFINITY_SHARE_UID;
import static android.view.Display.DEFAULT_DISPLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.ComponentName;
import android.platform.test.annotations.Presubmit;
import android.server.wm.annotation.Group3;

import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:ActivityTaskAffinityTests
 *
 * Tests if activities with the same taskAffinity can be in the same task.
 */
@Presubmit
@Group3
public class ActivityTaskAffinityTests extends ActivityManagerTestBase {
    @Test
    public void testActivitiesWithSameAffinityDifferentAppDifferentUidDifferentTask() {
        testActivitiesShouldBeInTheSameTask(
                TEST_ACTIVITY_WITH_SAME_AFFINITY,
                TEST_ACTIVITY_WITH_SAME_AFFINITY_DIFFERENT_UID,
                false /* sameTask */
        );
    }

    @Test
    public void testActivitiesWithSameAffinityUidDifferentAppSameTask() {
        testActivitiesShouldBeInTheSameTask(
                TEST_ACTIVITY_WITH_SAME_AFFINITY,
                TEST_ACTIVITY_WITH_SAME_AFFINITY_SHARE_UID,
                true /* sameTask */
        );
    }

    @Test
    public void testActivitiesWithSameAffinitySameAppSameTask() {
        testActivitiesShouldBeInTheSameTask(
                TEST_ACTIVITY_WITH_SAME_AFFINITY,
                TEST_ACTIVITY_WITH_SAME_AFFINITY_SAME_APP,
                true /* sameTask */
        );
    }

    private void testActivitiesShouldBeInTheSameTask(ComponentName activityA,
            ComponentName activityB, boolean sameTask) {
        launchActivity(activityA);
        waitAndAssertTopResumedActivity(activityA, DEFAULT_DISPLAY,
                "Launched activity must be top-resumed.");
        final int firstAppTaskId = mWmState.getTaskByActivity(activityA).mTaskId;

        launchActivity(activityB);
        waitAndAssertTopResumedActivity(activityB, DEFAULT_DISPLAY,
                "Launched activity must be top-resumed.");
        final int secondAppTaskId = mWmState.getTaskByActivity(activityB).mTaskId;

        if (sameTask) {
            assertEquals("Activities with same affinity must be in the same task.",
                    firstAppTaskId, secondAppTaskId);
        } else {
            assertNotEquals("Activities with same affinity but different uid must not be"
                    + " in the same task.", firstAppTaskId, secondAppTaskId);
        }
    }
}
