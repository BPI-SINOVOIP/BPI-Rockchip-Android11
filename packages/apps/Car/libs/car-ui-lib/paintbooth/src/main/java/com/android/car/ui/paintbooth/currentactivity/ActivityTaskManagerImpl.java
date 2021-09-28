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

package com.android.car.ui.paintbooth.currentactivity;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningTaskInfo;
import android.app.IActivityTaskManager;
import android.content.ComponentName;
import android.os.RemoteException;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class is a wrapper around {@link android.app.ActivityTaskManager} that is excluded from
 * the gradle and google3 builds.
 */
class ActivityTaskManagerImpl implements ActivityTaskManager {

    IActivityTaskManager mActivityTaskManager = android.app.ActivityTaskManager.getService();

    Map<TaskStackListener, android.app.TaskStackListener> mListenerMapping = new HashMap<>();

    @Override
    public List<RunningTaskInfo> getTasks(int maxNum) throws RemoteException {
        return mActivityTaskManager.getTasks(maxNum);
    }

    @Override
    public void registerTaskStackListener(TaskStackListener listener) throws RemoteException {
        mListenerMapping.put(listener, new android.app.TaskStackListener() {
            @Override
            public void onTaskCreated(int taskId, ComponentName componentName) {
                listener.onTaskCreated(taskId, componentName);
            }

            @Override
            public void onTaskRemoved(int taskId) {
                listener.onTaskRemoved(taskId);
            }

            @Override
            public void onTaskDescriptionChanged(ActivityManager.RunningTaskInfo taskInfo) {
                listener.onTaskDescriptionChanged(taskInfo);
            }

            @Override
            public void onTaskMovedToFront(ActivityManager.RunningTaskInfo taskInfo) {
                listener.onTaskMovedToFront(taskInfo);
            }
        });

        mActivityTaskManager.registerTaskStackListener(mListenerMapping.get(listener));
    }

    @Override
    public void unregisterTaskStackListener(TaskStackListener listener) throws RemoteException {
        mActivityTaskManager.unregisterTaskStackListener(mListenerMapping.get(listener));
        mListenerMapping.remove(listener);
    }

}
