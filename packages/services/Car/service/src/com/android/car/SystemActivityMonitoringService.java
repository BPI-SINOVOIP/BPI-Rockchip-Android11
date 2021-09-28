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
package com.android.car;

import static com.android.car.pm.CarPackageManagerService.BLOCKING_INTENT_EXTRA_DISPLAY_ID;

import android.app.ActivityManager;
import android.app.ActivityManager.StackInfo;
import android.app.ActivityOptions;
import android.app.IActivityManager;
import android.app.IProcessObserver;
import android.app.TaskStackListener;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;
import android.view.Display;

import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * Service to monitor AMS for new Activity or Service launching.
 */
public class SystemActivityMonitoringService implements CarServiceBase {

    /**
     * Container to hold info on top task in an Activity stack
     */
    public static class TopTaskInfoContainer {
        public final ComponentName topActivity;
        public final int taskId;
        public final int displayId;
        public final int position;
        public final StackInfo stackInfo;

        private TopTaskInfoContainer(ComponentName topActivity, int taskId,
                int displayId, int position, StackInfo stackInfo) {
            this.topActivity = topActivity;
            this.taskId = taskId;
            this.displayId = displayId;
            this.position = position;
            this.stackInfo = stackInfo;
        }

        public boolean isMatching(TopTaskInfoContainer taskInfo) {
            return taskInfo != null
                    && Objects.equals(this.topActivity, taskInfo.topActivity)
                    && this.taskId == taskInfo.taskId
                    && this.displayId == taskInfo.displayId
                    && this.position == taskInfo.position
                    && this.stackInfo.userId == taskInfo.stackInfo.userId;
        }

        @Override
        public String toString() {
            return String.format(
                    "TaskInfoContainer [topActivity=%s, taskId=%d, stackId=%d, userId=%d, "
                    + "displayId=%d, position=%d",
                  topActivity, taskId, stackInfo.stackId, stackInfo.userId, displayId, position);
        }
    }

    public interface ActivityLaunchListener {
        /**
         * Notify launch of activity.
         * @param topTask Task information for what is currently launched.
         */
        void onActivityLaunch(TopTaskInfoContainer topTask);
    }

    private static final int INVALID_STACK_ID = -1;
    private final Context mContext;
    private final IActivityManager mAm;
    private final ProcessObserver mProcessObserver;
    private final TaskListener mTaskListener;

    private final HandlerThread mMonitorHandlerThread = CarServiceUtils.getHandlerThread(
            getClass().getSimpleName());
    private final ActivityMonitorHandler mHandler = new ActivityMonitorHandler(
            mMonitorHandlerThread.getLooper(), this);

    private final Object mLock = new Object();

    /** K: display id, V: top task */
    @GuardedBy("mLock")
    private final SparseArray<TopTaskInfoContainer> mTopTasks = new SparseArray<>();
    /** K: uid, V : list of pid */
    @GuardedBy("mLock")
    private final Map<Integer, Set<Integer>> mForegroundUidPids = new ArrayMap<>();
    @GuardedBy("mLock")
    private ActivityLaunchListener mActivityLaunchListener;

    public SystemActivityMonitoringService(Context context) {
        mContext = context;
        mProcessObserver = new ProcessObserver();
        mTaskListener = new TaskListener();
        mAm = ActivityManager.getService();
    }

    @Override
    public void init() {
        // Monitoring both listeners are necessary as there are cases where one listener cannot
        // monitor activity change.
        try {
            mAm.registerProcessObserver(mProcessObserver);
            mAm.registerTaskStackListener(mTaskListener);
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "cannot register activity monitoring", e);
            throw new RuntimeException(e);
        }
        updateTasks();
    }

    @Override
    public void release() {
        try {
            mAm.unregisterProcessObserver(mProcessObserver);
            mAm.unregisterTaskStackListener(mTaskListener);
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "Failed to unregister listeners", e);
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*SystemActivityMonitoringService*");
        writer.println(" Top Tasks per display:");
        synchronized (mLock) {
            for (int i = 0; i < mTopTasks.size(); i++) {
                int displayId = mTopTasks.keyAt(i);
                TopTaskInfoContainer info = mTopTasks.valueAt(i);
                if (info != null) {
                    writer.println("display id " + displayId + ": " + info);
                }
            }
            writer.println(" Foreground uid-pids:");
            for (Integer key : mForegroundUidPids.keySet()) {
                Set<Integer> pids = mForegroundUidPids.get(key);
                if (pids == null) {
                    continue;
                }
                writer.println("uid:" + key + ", pids:" + Arrays.toString(pids.toArray()));
            }
        }
    }

    /**
     * Block the current task: Launch new activity with given Intent and finish the current task.
     * @param currentTask task to finish
     * @param newActivityIntent Intent for new Activity
     */
    public void blockActivity(TopTaskInfoContainer currentTask, Intent newActivityIntent) {
        mHandler.requestBlockActivity(currentTask, newActivityIntent);
    }

    public List<TopTaskInfoContainer> getTopTasks() {
        LinkedList<TopTaskInfoContainer> tasks = new LinkedList<>();
        synchronized (mLock) {
            for (int i = 0; i < mTopTasks.size(); i++) {
                TopTaskInfoContainer topTask = mTopTasks.valueAt(i);
                if (topTask == null) {
                    Log.e(CarLog.TAG_AM, "Top tasks contains null. Full content is: "
                            + mTopTasks.toString());
                    continue;
                }
                tasks.add(topTask);
            }
        }
        return tasks;
    }

    public boolean isInForeground(int pid, int uid) {
        synchronized (mLock) {
            Set<Integer> pids = mForegroundUidPids.get(uid);
            if (pids == null) {
                return false;
            }
            if (pids.contains(pid)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Attempts to restart a task.
     *
     * <p>Restarts a task by sending an empty intent with flag
     * {@link Intent#FLAG_ACTIVITY_CLEAR_TASK} to its root activity. If the task does not exist,
     * do nothing.
     *
     * @param taskId id of task to be restarted.
     */
    public void restartTask(int taskId) {
        String rootActivityName = null;
        int userId = 0;
        try {
            findRootActivityName:
            for (StackInfo info : mAm.getAllStackInfos()) {
                for (int i = 0; i < info.taskIds.length; i++) {
                    if (info.taskIds[i] == taskId) {
                        rootActivityName = info.taskNames[i];
                        userId = info.userId;
                        if (Log.isLoggable(CarLog.TAG_AM, Log.DEBUG)) {
                            Log.d(CarLog.TAG_AM, "Root activity is " + rootActivityName);
                            Log.d(CarLog.TAG_AM, "User id is " + userId);
                        }
                        // Break out of nested loop.
                        break findRootActivityName;
                    }
                }
            }
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "Could not get stack info", e);
            return;
        }

        if (rootActivityName == null) {
            Log.e(CarLog.TAG_AM, "Could not find root activity with task id " + taskId);
            return;
        }

        Intent rootActivityIntent = new Intent();
        rootActivityIntent.setComponent(ComponentName.unflattenFromString(rootActivityName));
        // Clear the task the root activity is running in and start it in a new task.
        // Effectively restart root activity.
        rootActivityIntent.addFlags(
                Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);

        if (Log.isLoggable(CarLog.TAG_AM, Log.INFO)) {
            Log.i(CarLog.TAG_AM, "restarting root activity with user id " + userId);
        }
        mContext.startActivityAsUser(rootActivityIntent, new UserHandle(userId));
    }

    public void registerActivityLaunchListener(ActivityLaunchListener listener) {
        synchronized (mLock) {
            mActivityLaunchListener = listener;
        }
    }

    private void updateTasks() {
        List<StackInfo> infos;
        try {
            infos = mAm.getAllStackInfos();
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "cannot getTasks", e);
            return;
        }

        if (infos == null) {
            return;
        }

        int focusedStackId = INVALID_STACK_ID;
        try {
            // TODO(b/66955160): Someone on the Auto-team should probably re-work the code in the
            // synchronized block below based on this new API.
            final StackInfo focusedStackInfo = mAm.getFocusedStackInfo();
            if (focusedStackInfo != null) {
                focusedStackId = focusedStackInfo.stackId;
            }
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "cannot getFocusedStackId", e);
            return;
        }

        SparseArray<TopTaskInfoContainer> topTasks = new SparseArray<>();
        ActivityLaunchListener listener;
        synchronized (mLock) {
            mTopTasks.clear();
            listener = mActivityLaunchListener;

            for (StackInfo info : infos) {
                int displayId = info.displayId;
                if (info.taskNames.length == 0 || !info.visible) { // empty stack or not shown
                    continue;
                }
                TopTaskInfoContainer newTopTaskInfo = new TopTaskInfoContainer(
                        info.topActivity, info.taskIds[info.taskIds.length - 1],
                        info.displayId, info.position, info);
                TopTaskInfoContainer currentTopTaskInfo = topTasks.get(displayId);

                if (currentTopTaskInfo == null ||
                        newTopTaskInfo.position > currentTopTaskInfo.position) {
                    topTasks.put(displayId, newTopTaskInfo);
                    if (Log.isLoggable(CarLog.TAG_AM, Log.INFO)) {
                        Log.i(CarLog.TAG_AM, "Updating top task to: " + newTopTaskInfo);
                    }
                }
            }
            // Assuming displays remains the same.
            for (int i = 0; i < topTasks.size(); i++) {
                TopTaskInfoContainer topTask = topTasks.valueAt(i);

                int displayId = topTasks.keyAt(i);
                mTopTasks.put(displayId, topTask);
            }
        }
        if (listener != null) {
            for (int i = 0; i < topTasks.size(); i++) {
                TopTaskInfoContainer topTask = topTasks.valueAt(i);

                if (Log.isLoggable(CarLog.TAG_AM, Log.INFO)) {
                    Log.i(CarLog.TAG_AM, "Notifying about top task: " + topTask.toString());
                }
                listener.onActivityLaunch(topTask);
            }
        }
    }

    public StackInfo getFocusedStackForTopActivity(ComponentName activity) {
        StackInfo focusedStack;
        try {
            focusedStack = mAm.getFocusedStackInfo();
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "cannot getFocusedStackId", e);
            return null;
        }
        if (focusedStack.taskNames.length == 0) { // nothing in focused stack
            return null;
        }
        ComponentName topActivity = ComponentName.unflattenFromString(
                focusedStack.taskNames[focusedStack.taskNames.length - 1]);
        if (topActivity.equals(activity)) {
            return focusedStack;
        } else {
            return null;
        }
    }

    private void handleForegroundActivitiesChanged(int pid, int uid, boolean foregroundActivities) {
        synchronized (mLock) {
            if (foregroundActivities) {
                Set<Integer> pids = mForegroundUidPids.get(uid);
                if (pids == null) {
                    pids = new ArraySet<Integer>();
                    mForegroundUidPids.put(uid, pids);
                }
                pids.add(pid);
            } else {
                doHandlePidGoneLocked(pid, uid);
            }
        }
    }

    private void handleProcessDied(int pid, int uid) {
        synchronized (mLock) {
            doHandlePidGoneLocked(pid, uid);
        }
    }

    private void doHandlePidGoneLocked(int pid, int uid) {
        Set<Integer> pids = mForegroundUidPids.get(uid);
        if (pids != null) {
            pids.remove(pid);
            if (pids.isEmpty()) {
                mForegroundUidPids.remove(uid);
            }
        }
    }

    /**
     * block the current task with the provided new activity.
     */
    private void handleBlockActivity(TopTaskInfoContainer currentTask, Intent newActivityIntent) {
        int displayId = newActivityIntent.getIntExtra(BLOCKING_INTENT_EXTRA_DISPLAY_ID,
                Display.DEFAULT_DISPLAY);
        if (Log.isLoggable(CarLog.TAG_AM, Log.DEBUG)) {
            Log.d(CarLog.TAG_AM, "Launching blocking activity on display: " + displayId);
        }

        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(displayId);
        mContext.startActivityAsUser(newActivityIntent, options.toBundle(),
                new UserHandle(currentTask.stackInfo.userId));
        // Now make stack with new activity focused.
        findTaskAndGrantFocus(newActivityIntent.getComponent());
    }

    private void findTaskAndGrantFocus(ComponentName activity) {
        List<StackInfo> infos;
        try {
            infos = mAm.getAllStackInfos();
        } catch (RemoteException e) {
            Log.e(CarLog.TAG_AM, "cannot getTasks", e);
            return;
        }
        for (StackInfo info : infos) {
            if (info.taskNames.length == 0) {
                continue;
            }
            ComponentName topActivity = ComponentName.unflattenFromString(
                    info.taskNames[info.taskNames.length - 1]);
            if (activity.equals(topActivity)) {
                try {
                    mAm.setFocusedStack(info.stackId);
                } catch (RemoteException e) {
                    Log.e(CarLog.TAG_AM, "cannot setFocusedStack to stack:" + info.stackId, e);
                }
                return;
            }
        }
        Log.i(CarLog.TAG_AM, "cannot give focus, cannot find Activity:" + activity);
    }

    private class ProcessObserver extends IProcessObserver.Stub {
        @Override
        public void onForegroundActivitiesChanged(int pid, int uid, boolean foregroundActivities) {
            if (Log.isLoggable(CarLog.TAG_AM, Log.INFO)) {
                Log.i(CarLog.TAG_AM,
                        String.format("onForegroundActivitiesChanged uid %d pid %d fg %b",
                    uid, pid, foregroundActivities));
            }
            mHandler.requestForegroundActivitiesChanged(pid, uid, foregroundActivities);
        }

        @Override
        public void onForegroundServicesChanged(int pid, int uid, int fgServiceTypes) {
        }

        @Override
        public void onProcessDied(int pid, int uid) {
            mHandler.requestProcessDied(pid, uid);
        }
    }

    private class TaskListener extends TaskStackListener {
        @Override
        public void onTaskStackChanged() {
            if (Log.isLoggable(CarLog.TAG_AM, Log.INFO)) {
                Log.i(CarLog.TAG_AM, "onTaskStackChanged");
            }
            mHandler.requestUpdatingTask();
        }
    }

    private static final class ActivityMonitorHandler extends Handler {
        private  static final String TAG = ActivityMonitorHandler.class.getSimpleName();

        private static final int MSG_UPDATE_TASKS = 0;
        private static final int MSG_FOREGROUND_ACTIVITIES_CHANGED = 1;
        private static final int MSG_PROCESS_DIED = 2;
        private static final int MSG_BLOCK_ACTIVITY = 3;

        private final WeakReference<SystemActivityMonitoringService> mService;

        private ActivityMonitorHandler(Looper looper, SystemActivityMonitoringService service) {
            super(looper);
            mService = new WeakReference<SystemActivityMonitoringService>(service);
        }

        private void requestUpdatingTask() {
            Message msg = obtainMessage(MSG_UPDATE_TASKS);
            sendMessage(msg);
        }

        private void requestForegroundActivitiesChanged(int pid, int uid,
                boolean foregroundActivities) {
            Message msg = obtainMessage(MSG_FOREGROUND_ACTIVITIES_CHANGED, pid, uid,
                    Boolean.valueOf(foregroundActivities));
            sendMessage(msg);
        }

        private void requestProcessDied(int pid, int uid) {
            Message msg = obtainMessage(MSG_PROCESS_DIED, pid, uid);
            sendMessage(msg);
        }

        private void requestBlockActivity(TopTaskInfoContainer currentTask,
                Intent newActivityIntent) {
            Message msg = obtainMessage(MSG_BLOCK_ACTIVITY,
                    new Pair<TopTaskInfoContainer, Intent>(currentTask, newActivityIntent));
            sendMessage(msg);
        }

        @Override
        public void handleMessage(Message msg) {
            SystemActivityMonitoringService service = mService.get();
            if (service == null) {
                Log.i(TAG, "handleMessage null service");
                return;
            }
            switch (msg.what) {
                case MSG_UPDATE_TASKS:
                    service.updateTasks();
                    break;
                case MSG_FOREGROUND_ACTIVITIES_CHANGED:
                    service.handleForegroundActivitiesChanged(msg.arg1, msg.arg2,
                            (Boolean) msg.obj);
                    service.updateTasks();
                    break;
                case MSG_PROCESS_DIED:
                    service.handleProcessDied(msg.arg1, msg.arg2);
                    break;
                case MSG_BLOCK_ACTIVITY:
                    Pair<TopTaskInfoContainer, Intent> pair =
                        (Pair<TopTaskInfoContainer, Intent>) msg.obj;
                    service.handleBlockActivity(pair.first, pair.second);
                    break;
            }
        }
    }
}
