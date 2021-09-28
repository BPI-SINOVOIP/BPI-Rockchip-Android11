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
import android.content.ComponentName;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.List;

/**
 * Interface for wrappers around {@link android.app.ActivityTaskManager} so that we can exclude them
 * from non-system builds like gradle and google3.
 */
interface ActivityTaskManager {

    interface TaskStackListener {
        void onTaskCreated(int taskId, ComponentName componentName);

        void onTaskRemoved(int taskId);

        void onTaskDescriptionChanged(ActivityManager.RunningTaskInfo taskInfo);

        void onTaskMovedToFront(ActivityManager.RunningTaskInfo taskInfo);
    }

    List<RunningTaskInfo> getTasks(int maxNum) throws RemoteException;

    void registerTaskStackListener(TaskStackListener listener) throws RemoteException;

    void unregisterTaskStackListener(TaskStackListener listener) throws RemoteException;

    final class ActivityTaskManagerStub implements ActivityTaskManager {
        @Override
        public List<RunningTaskInfo> getTasks(int num) throws RemoteException {
            throw new RemoteException("ActivityTaskManager is not available");
        }

        @Override
        public void registerTaskStackListener(TaskStackListener listener) throws RemoteException {
            throw new RemoteException("ActivityTaskManager is not available");
        }

        @Override
        public void unregisterTaskStackListener(TaskStackListener listener) throws RemoteException {
            throw new RemoteException("ActivityTaskManager is not available");
        }
    }

    @NonNull
    static ActivityTaskManager getService() {
        try {
            Class<?> clazz = Class.forName(
                    "com.android.car.ui.paintbooth.currentactivity.ActivityTaskManagerImpl");
            Constructor<?> constructor = clazz.getDeclaredConstructor();
            constructor.setAccessible(true);
            return (ActivityTaskManager) constructor.newInstance();
        } catch (ClassNotFoundException | IllegalAccessException | InstantiationException
                | NoSuchMethodException | InvocationTargetException e) {
            Log.e("paintbooth", "ActivityTaskManager is not available", e);
            return new ActivityTaskManagerStub();
        }
    }
}
