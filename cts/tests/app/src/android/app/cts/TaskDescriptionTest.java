/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.app.cts;

import static android.content.Context.ACTIVITY_SERVICE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.ActivityManager.TaskDescription;
import android.app.stubs.MockActivity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.SystemClock;
import android.platform.test.annotations.Presubmit;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Build & Run: atest android.app.cts.TaskDescriptionTest
 */
@RunWith(AndroidJUnit4.class)
@Presubmit
public class TaskDescriptionTest {
    private static final String TEST_LABEL = "test-label";
    private static final int TEST_NO_DATA = 0;
    private static final int TEST_RES_DATA = 777;
    private static final int TEST_COLOR = Color.BLACK;
    private static final int WAIT_TIMEOUT_MS = 1000;
    private static final int WAIT_RETRIES = 5;

    @Rule
    public ActivityTestRule<MockActivity> mTaskDescriptionActivity =
            new ActivityTestRule<>(MockActivity.class,
                    false /* initialTouchMode */, false /* launchActivity */);

    @Test
    public void testBitmapConstructor() throws Exception {
        final Activity activity = mTaskDescriptionActivity.launchActivity(null);
        final Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(0);
        activity.setTaskDescription(new TaskDescription(TEST_LABEL, bitmap, TEST_COLOR));
        assertTaskDescription(activity, TEST_LABEL, TEST_NO_DATA, bitmap);

        activity.setTaskDescription(new TaskDescription(TEST_LABEL, bitmap));
        assertTaskDescription(activity, TEST_LABEL, TEST_NO_DATA, bitmap);
    }

    @Test
    public void testResourceConstructor() throws Exception {
        final Activity activity = mTaskDescriptionActivity.launchActivity(null);
        activity.setTaskDescription(new TaskDescription(TEST_LABEL, TEST_RES_DATA, TEST_COLOR));
        assertTaskDescription(activity, TEST_LABEL, TEST_RES_DATA, null);

        activity.setTaskDescription(new TaskDescription(TEST_LABEL, TEST_RES_DATA));
        assertTaskDescription(activity, TEST_LABEL, TEST_RES_DATA, null);
    }

    @Test
    public void testLabelConstructor() throws Exception {
        final Activity activity = mTaskDescriptionActivity.launchActivity(null);
        activity.setTaskDescription(new TaskDescription(TEST_LABEL));
        assertTaskDescription(activity, TEST_LABEL, TEST_NO_DATA, null);
    }

    @Test
    public void testEmptyConstructor() throws Exception {
        final Activity activity = mTaskDescriptionActivity.launchActivity(null);
        activity.setTaskDescription(new TaskDescription());
        assertTaskDescription(activity, null, TEST_NO_DATA, null);
    }

    private void assertTaskDescription(Activity activity, String label, int resId, Bitmap bitmap) {
        final ActivityManager am = (ActivityManager) activity.getSystemService(ACTIVITY_SERVICE);
        List<RecentTaskInfo> recentsTasks = am.getRecentTasks(1 /* maxNum */, 0 /* flags */);
        if (!recentsTasks.isEmpty()) {
            final RecentTaskInfo info = recentsTasks.get(0);
            if (activity.getTaskId() == info.id) {
                final TaskDescription td = info.taskDescription;
                assertNotNull(td);
                if (bitmap != null) {
                    // TaskPersister at the worst case scenario waits 3 secs (PRE_TASK_DELAY_MS) to
                    // write the image to disk if its write time has ended
                    waitFor("TaskDescription's icon is null", () -> td.getIcon() != null);
                    waitFor("TaskDescription's icon filename is null",
                            () -> td.getIconFilename() != null);
                } else {
                    waitFor("TaskDescription's icon is not null", () -> td.getIcon() == null);
                    waitFor("TaskDescription's icon filename is not null",
                            () -> td.getIconFilename() == null);
                }

                assertEquals(resId, td.getIconResource());
                assertEquals(label, td.getLabel());
                return;
            }
        }
        fail("Did not find activity (id=" + activity.getTaskId() + ") in recent tasks list");
    }

    private void waitFor(String message, BooleanSupplier waitCondition) {
        for (int retry = 0; retry < WAIT_RETRIES; retry++) {
            if (waitCondition.getAsBoolean()) {
                return;
            }
            SystemClock.sleep(WAIT_TIMEOUT_MS);
        }
        fail(message);
    }
}
