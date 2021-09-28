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

package android.app.cts;

import static android.content.Context.ACTIVITY_SERVICE;
import static android.content.Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Instrumentation;
import android.app.stubs.MockActivity;
import android.app.stubs.MockListActivity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.runner.lifecycle.ActivityLifecycleCallback;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitor;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * atest CtsAppTestCases:AppTaskTests
 */
@MediumTest
@FlakyTest(detail = "Can be promoted to pre-submit once confirmed stable.")
@RunWith(AndroidJUnit4.class)
public class AppTaskTests {

    private static final long TIME_SLICE_MS = 100;
    private static final long MAX_WAIT_MS = 1500;

    private Instrumentation mInstrumentation;
    private ActivityLifecycleMonitor mLifecycleMonitor;
    private Context mTargetContext;

    @Rule
    public ActivityTestRule<MockActivity> mActivityRule =
            new ActivityTestRule<MockActivity>(MockActivity.class) {
        @Override
        public Intent getActivityIntent() {
            Intent intent = new Intent();
            intent.setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_NEW_DOCUMENT
                    | FLAG_ACTIVITY_MULTIPLE_TASK);
            return intent;
        }
    };

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mLifecycleMonitor = ActivityLifecycleMonitorRegistry.getInstance();
        mTargetContext = mInstrumentation.getTargetContext();
        removeAllAppTasks();
    }

    /**
     * Launch an activity and ensure it is in the app task list.
     */
    @Test
    public void testSingleAppTask() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        final List<ActivityManager.AppTask> appTasks = getAppTasks();
        assertTrue(appTasks.size() == 1);
        assertTrue(appTasks.get(0).getTaskInfo().topActivity.equals(a1.getComponentName()));
    }

    /**
     * Launch a couple tasks and ensure they are in the app tasks list.
     */
    @Test
    public void testMultipleAppTasks() throws Exception {
        final ArrayList<Activity> activities = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            activities.add(mActivityRule.launchActivity(null));
        }
        final List<ActivityManager.AppTask> appTasks = getAppTasks();
        assertTrue(appTasks.size() == activities.size());
        for (int i = 0; i < appTasks.size(); i++) {
            assertTrue(appTasks.get(i).getTaskInfo().topActivity.equals(
                    activities.get(i).getComponentName()));
        }
    }

    /**
     * Remove an app task and ensure that it is actually removed.
     */
    @Test
    public void testFinishAndRemoveTask() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        waitAndAssertCondition(() -> getAppTasks().size() == 1, "Expected 1 running task");
        getAppTask(a1).finishAndRemoveTask();
        waitAndAssertCondition(() -> getAppTasks().isEmpty(), "Expected no running tasks");
    }

    /**
     * Ensure that moveToFront will bring the first activity forward.
     */
    @Test
    public void testMoveToFront() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        final Activity a2 = mActivityRule.launchActivity(null);
        final BooleanValue targetResumed = new BooleanValue();
        mLifecycleMonitor.addLifecycleCallback(new ActivityLifecycleCallback() {
            public void onActivityLifecycleChanged(Activity activity, Stage stage) {
                if (activity == a1 && stage == Stage.RESUMED) {
                    targetResumed.value = true;
                }
            }
        });

        getAppTask(a1).moveToFront();
        waitAndAssertCondition(() -> targetResumed.value,
                "Expected activity brought to front and resumed");
    }

    /**
     * Ensure that starting a new activity in the same task results in two activities in the task.
     */
    @Test
    public void testStartActivityInTask_NoNewTask() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        final ActivityManager.AppTask task = getAppTask(a1);
        final Intent intent = new Intent();
        intent.setComponent(new ComponentName(mTargetContext, MockListActivity.class));
        task.startActivity(mTargetContext, intent, null);
        waitAndAssertCondition(
                () -> getAppTask(a1) != null && getAppTask(a1).getTaskInfo().numActivities == 2,
                "Waiting for activity launch");

        final ActivityManager.RecentTaskInfo taskInfo = task.getTaskInfo();
        assertTrue(taskInfo.numActivities == 2);
        assertTrue(taskInfo.baseActivity.equals(a1.getComponentName()));
        assertTrue(taskInfo.topActivity.equals(intent.getComponent()));
    }

    /**
     * Ensure that an activity with FLAG_ACTIVITY_NEW_TASK causes the task to be brought forward
     * and the new activity not being started.
     */
    @Test
    public void testStartActivityInTask_NewTask() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        final ActivityManager.AppTask task = getAppTask(a1);
        final Intent intent = new Intent();
        intent.setComponent(new ComponentName(mTargetContext, MockActivity.class));
        intent.setFlags(FLAG_ACTIVITY_NEW_TASK);
        task.startActivity(mTargetContext, intent, null);

        final ActivityManager.RecentTaskInfo taskInfo = task.getTaskInfo();
        assertTrue(taskInfo.numActivities == 1);
        assertTrue(taskInfo.baseActivity.equals(a1.getComponentName()));
    }

    /**
     * Ensure that the activity that is excluded from recents is reflected in the recent task info.
     */
    @Test
    public void testSetExcludeFromRecents() throws Exception {
        final Activity a1 = mActivityRule.launchActivity(null);
        final List<ActivityManager.AppTask> appTasks = getAppTasks();
        final ActivityManager.AppTask t1 = appTasks.get(0);
        t1.setExcludeFromRecents(true);
        assertTrue((t1.getTaskInfo().baseIntent.getFlags() & FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS)
                != 0);
        t1.setExcludeFromRecents(false);
        assertTrue((t1.getTaskInfo().baseIntent.getFlags() & FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS)
                == 0);
    }

    /**
     * @return all the {@param ActivityManager.AppTask}s for the current app.
     */
    private List<ActivityManager.AppTask> getAppTasks() {
        ActivityManager am = (ActivityManager) mTargetContext.getSystemService(ACTIVITY_SERVICE);
        return am.getAppTasks();
    }

    /**
     * @return the {@param ActivityManager.AppTask} for the associated activity.
     */
    private ActivityManager.AppTask getAppTask(Activity activity) {
        waitAndAssertCondition(() -> getAppTask(getAppTasks(), activity) != null,
                "Waiting for app task");
        return getAppTask(getAppTasks(), activity);
    }

    private ActivityManager.AppTask getAppTask(List<ActivityManager.AppTask> appTasks,
            Activity activity) {
        for (ActivityManager.AppTask task : appTasks) {
            if (task.getTaskInfo().taskId == activity.getTaskId()) {
                return task;
            }
        }
        return null;
    }

    /**
     * Removes all the app tasks the test app.
     */
    private void removeAllAppTasks() {
        final List<ActivityManager.AppTask> appTasks = getAppTasks();
        for (ActivityManager.AppTask task : appTasks) {
            task.finishAndRemoveTask();
        }
        waitAndAssertCondition(() -> getAppTasks().isEmpty(),
                "Expected no app tasks after all removed");
    }

    private void waitAndAssertCondition(BooleanSupplier condition, String failMsgContext) {
        long startTime = SystemClock.elapsedRealtime();
        while (true) {
            if (condition.getAsBoolean()) {
                // Condition passed
                return;
            } else if (SystemClock.elapsedRealtime() > (startTime + MAX_WAIT_MS)) {
                // Timed out
                fail("Timed out waiting for: " + failMsgContext);
            } else {
                SystemClock.sleep(TIME_SLICE_MS);
            }
        }
    }

    private class BooleanValue {
        boolean value;
    }
}