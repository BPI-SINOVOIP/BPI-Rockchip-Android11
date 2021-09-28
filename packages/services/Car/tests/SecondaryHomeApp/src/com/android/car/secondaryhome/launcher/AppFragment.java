/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.StackInfo;
import android.app.ActivityView;
import android.app.IActivityManager;
import android.app.TaskStackBuilder;
import android.app.TaskStackListener;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.hardware.input.InputManager;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import com.android.car.secondaryhome.R;
import com.android.internal.annotations.GuardedBy;

/**
 * {@link Fragment} that contains an ActivityView to run embedded application
 */
public final class AppFragment extends Fragment {
    public static final int INVALID_TASK_STACK_ID = -1;

    private static final String TAG = "SecHome.AppFragment";

    private final IActivityManager mAm;
    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private int mVirtualDisplayId = Display.INVALID_DISPLAY;
    @GuardedBy("mLock")
    private int mTaskStackId = INVALID_TASK_STACK_ID;
    private boolean mActivityViewReady;
    private Intent mLaunchIntent;
    private TaskListener mTaskListener;
    private TaskStackBuilder mTaskStackBuilder;

    private Activity mActivity;
    private ActivityView mActivityView;
    private int mHomeDisplayId = Display.INVALID_DISPLAY;

    private final ActivityView.StateCallback mActivityViewCallback =
            new ActivityView.StateCallback() {
                @Override
                public void onActivityViewReady(ActivityView view) {
                    mActivityViewReady = true;
                    view.startActivity(mLaunchIntent);
                    synchronized (mLock) {
                        try {
                            mTaskStackId = mAm.getFocusedStackInfo().stackId;
                        } catch (RemoteException e) {
                            Log.e(TAG, "cannot get new taskstackid in ActivityView.StateCallback");
                        }
                        mVirtualDisplayId = view.getVirtualDisplayId();
                    }
                }

                @Override
                public void onActivityViewDestroyed(ActivityView view) {
                    mActivityViewReady = false;
                }
            };

    public AppFragment() {
        mAm = ActivityManager.getService();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mActivity = getActivity();
        mHomeDisplayId = mActivity.getWindowManager().getDefaultDisplay().getDisplayId();

        mTaskListener = new TaskListener();
        mTaskStackBuilder = TaskStackBuilder.create(mActivity);

        try {
            mAm.registerTaskStackListener(mTaskListener);
        } catch (RemoteException e) {
            mTaskListener = null;
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.app_fragment, container, false);
        mActivityView = view.findViewById(R.id.app_view);
        return view;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mActivityView != null && mActivityViewReady) {
            try {
                mActivityView.release();
                mActivityView = null;
                mActivityViewReady = false;
            } catch (Exception e) {
                Log.e(TAG, "Fail to release ActivityView");
            }
        }

        if (mTaskListener != null) {
            mLaunchIntent = null;
            mTaskListener = null;
            synchronized (mLock) {
                mTaskStackId = INVALID_TASK_STACK_ID;
                mVirtualDisplayId = Display.INVALID_DISPLAY;
            }
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        // If task stack is not empty, launch the top intent
        if (mTaskStackBuilder.getIntentCount() > 0) {
            launchTopAppInActivityView();
        }
    }

    private void launchTopAppInActivityView() {
        try {
            if (mTaskStackBuilder.getIntentCount() == 0) {
                return;
            }
            mLaunchIntent = mTaskStackBuilder
                    .editIntentAt(mTaskStackBuilder.getIntentCount() - 1);

            if (mActivityView != null) {
                // Set callback to launch the app when ActivityView is ready
                mActivityView.setCallback(mActivityViewCallback);
            } else if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "ActivityView is null ");
            }
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "App activity not found ..", e);
        }
    }

    public void addLaunchIntentToStack(Intent launchIntent) {
        mTaskStackBuilder.addNextIntent(launchIntent);
        launchTopAppInActivityView();
    }

    public void onBackButtonPressed() {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onBackButtonPressed..");
        }
        if (mActivityView != null) {
            performBackPress();
        }
    }

    public int getTaskStackId() {
        synchronized (mLock) {
            return mTaskStackId;
        }
    }

    private void performBackPress() {
        InputManager im = mActivity.getSystemService(InputManager.class);
        int displayId;
        synchronized (mLock) {
            displayId = mVirtualDisplayId;
        }

        im.injectInputEvent(createKeyEvent(KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_BACK, displayId),
                InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
        im.injectInputEvent(createKeyEvent(KeyEvent.ACTION_UP,
                KeyEvent.KEYCODE_BACK, displayId),
                InputManager.INJECT_INPUT_EVENT_MODE_ASYNC);
    }

    private static KeyEvent createKeyEvent(int action, int code, int displayId) {
        long when = SystemClock.uptimeMillis();
        KeyEvent ev = new KeyEvent(when, when, action, code, /* repeat= */ 0,
                /* metaState= */ 0, KeyCharacterMap.VIRTUAL_KEYBOARD, /* scancode= */ 0,
                KeyEvent.FLAG_FROM_SYSTEM | KeyEvent.FLAG_VIRTUAL_HARD_KEY,
                InputDevice.SOURCE_KEYBOARD);
        ev.setDisplayId(displayId);
        return ev;
    }

    private boolean isTaskStackEmpty() {
        synchronized (mLock) {
            try {
                return mAm.getAllStackInfos().stream().anyMatch(info
                        -> (info.stackId == mTaskStackId && info.topActivity == null));
            } catch (RemoteException e) {
                Log.e(TAG, "cannot getFocusedStackInfos", e);
                return true;
            }
        }
    }

    private final class TaskListener extends TaskStackListener {
        @Override
        public void onTaskStackChanged() {
            StackInfo focusedStackInfo;
            try {
                focusedStackInfo = mAm.getFocusedStackInfo();
            } catch (RemoteException e) {
                Log.e(TAG, "cannot getFocusedStackInfo", e);
                return;
            }

            // App could be exited in two ways, and HomeFragment should be shown
            if (isTaskStackEmpty()) {
                ((LauncherActivity) mActivity).navigateHome();
                synchronized (mLock) {
                    mTaskStackId = INVALID_TASK_STACK_ID;
                }
            }

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "OnTaskStackChanged: virtual display: "
                        + mVirtualDisplayId + " homeDisplay: " + mHomeDisplayId
                        + "\nFocused stack: " + focusedStackInfo);
                try {
                    for (StackInfo info: mAm.getAllStackInfos()) {
                        Log.d(TAG, "    stackId: " + info.stackId + " displayId: "
                                + info.displayId + " component: " + info.topActivity);
                    }
                } catch (RemoteException e) {
                    Log.e(TAG, "cannot getFocusedStackInfos", e);
                }
            }
        }
    }
}
