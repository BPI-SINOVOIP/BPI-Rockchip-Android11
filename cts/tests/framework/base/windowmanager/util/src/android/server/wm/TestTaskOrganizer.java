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

package android.server.wm;

import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
import static android.view.Display.DEFAULT_DISPLAY;

import android.app.ActivityManager;
import android.view.Display;
import android.view.SurfaceControl;
import android.window.TaskOrganizer;
import android.window.WindowContainerTransaction;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

class TestTaskOrganizer extends TaskOrganizer {

    private boolean mRegistered;
    final HashMap<Integer, ActivityManager.RunningTaskInfo> mKnownTasks = new HashMap<>();
    private ActivityManager.RunningTaskInfo mRootPrimary;
    private boolean mRootPrimaryHasChild;
    private ActivityManager.RunningTaskInfo mRootSecondary;

    private void registerOrganizerIfNeeded() {
        if (mRegistered) return;

        registerOrganizer(WINDOWING_MODE_SPLIT_SCREEN_PRIMARY);
        registerOrganizer(WINDOWING_MODE_SPLIT_SCREEN_SECONDARY);
        mRegistered = true;
    }

    void unregisterOrganizerIfNeeded() {
        if (!mRegistered) return;
        mRegistered = false;

        dismissedSplitScreen();
        super.unregisterOrganizer();
    }

    void putTaskInSplitPrimary(int taskId) {
        registerOrganizerIfNeeded();
        ActivityManager.RunningTaskInfo taskInfo = getTaskInfo(taskId);
        final WindowContainerTransaction t = new WindowContainerTransaction();
        t.reparent(taskInfo.getToken(), mRootPrimary.getToken(), true /* onTop */);
        applyTransaction(t);
    }

    void dismissedSplitScreen() {
        // Re-set default launch root.
        TaskOrganizer.setLaunchRoot(Display.DEFAULT_DISPLAY, null);

        // Re-parent everything back to the display from the splits so that things are as they were.
        final List<ActivityManager.RunningTaskInfo> children = new ArrayList<>();
        final List<ActivityManager.RunningTaskInfo> primaryChildren =
                getChildTasks(mRootPrimary.getToken(), null /* activityTypes */);
        if (primaryChildren != null && !primaryChildren.isEmpty()) {
            children.addAll(primaryChildren);
        }
        final List<ActivityManager.RunningTaskInfo> secondaryChildren =
                getChildTasks(mRootSecondary.getToken(), null /* activityTypes */);
        if (secondaryChildren != null && !secondaryChildren.isEmpty()) {
            children.addAll(secondaryChildren);
        }
        if (children.isEmpty()) {
            return;
        }

        final WindowContainerTransaction t = new WindowContainerTransaction();
        for (ActivityManager.RunningTaskInfo task : children) {
            t.reparent(task.getToken(), null /* parent */, true /* onTop */);
        }
        applyTransaction(t);
    }

    /** Also completes the process of entering split mode. */
    private void processRootPrimaryTaskInfoChanged() {
        List<ActivityManager.RunningTaskInfo> children =
                getChildTasks(mRootPrimary.getToken(), null /* activityTypes */);
        final boolean hasChild = !children.isEmpty();
        if (mRootPrimaryHasChild == hasChild) return;
        mRootPrimaryHasChild = hasChild;
        if (!hasChild) return;

        // Finish entering split-screen mode

        // Set launch root for the default display to secondary...for no good reason...
        setLaunchRoot(DEFAULT_DISPLAY, mRootSecondary.getToken());

        List<ActivityManager.RunningTaskInfo> rootTasks =
                getRootTasks(DEFAULT_DISPLAY, null /* activityTypes */);
        if (rootTasks.isEmpty()) return;
        // Move all root fullscreen task to secondary split.
        final WindowContainerTransaction t = new WindowContainerTransaction();
        for (int i = rootTasks.size() - 1; i >= 0; --i) {
            final ActivityManager.RunningTaskInfo task = rootTasks.get(i);
            if (task.getConfiguration().windowConfiguration.getWindowingMode()
                    == WINDOWING_MODE_FULLSCREEN) {
                t.reparent(task.getToken(), mRootSecondary.getToken(), true /* onTop */);
            }
        }
        // Move the secondary split-forward.
        t.reorder(mRootSecondary.getToken(), true /* onTop */);
        applyTransaction(t);
    }

    private ActivityManager.RunningTaskInfo getTaskInfo(int taskId) {
        ActivityManager.RunningTaskInfo taskInfo = mKnownTasks.get(taskId);
        if (taskInfo != null) return taskInfo;

        final List<ActivityManager.RunningTaskInfo> rootTasks = getRootTasks(DEFAULT_DISPLAY, null);
        for (ActivityManager.RunningTaskInfo info : rootTasks) {
            addTask(info);
        }

        return mKnownTasks.get(taskId);
    }

    @Override
    public void onTaskAppeared(@NonNull ActivityManager.RunningTaskInfo taskInfo,
            SurfaceControl leash) {
        addTask(taskInfo);
    }

    @Override
    public void onTaskVanished(@NonNull ActivityManager.RunningTaskInfo taskInfo) {
        removeTask(taskInfo);
    }

    @Override
    public void onTaskInfoChanged(ActivityManager.RunningTaskInfo taskInfo) {
        addTask(taskInfo);
    }

    private void addTask(ActivityManager.RunningTaskInfo taskInfo) {
        mKnownTasks.put(taskInfo.taskId, taskInfo);

        final int windowingMode =
                taskInfo.getConfiguration().windowConfiguration.getWindowingMode();
        if (windowingMode == WINDOWING_MODE_SPLIT_SCREEN_PRIMARY) {
            mRootPrimary = taskInfo;
            processRootPrimaryTaskInfoChanged();
        }

        if (windowingMode == WINDOWING_MODE_SPLIT_SCREEN_SECONDARY) {
            mRootSecondary = taskInfo;
        }
    }

    private void removeTask(ActivityManager.RunningTaskInfo taskInfo) {
        final int taskId = taskInfo.taskId;
        // ignores cleanup on duplicated removal request
        if (mKnownTasks.remove(taskId) != null) {
            if (mRootPrimary != null && taskId == mRootPrimary.taskId) mRootPrimary = null;
            if (mRootSecondary != null && taskId == mRootSecondary.taskId) mRootSecondary = null;
        }
    }
}
