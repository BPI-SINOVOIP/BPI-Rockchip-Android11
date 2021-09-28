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

package android.car.app;

import android.annotation.Nullable;
import android.app.ActivityView;
import android.car.Car;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.content.ComponentName;
import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Display;

/**
 * CarActivityView is a special kind of ActivityView that can track which display the ActivityView
 * is placed.  This information can be used to enforce the driving safety.
 *
 * @hide
 */
public final class CarActivityView extends ActivityView {

    private static final String TAG = CarActivityView.class.getSimpleName();

    // volatile, since mUserActivityViewCallback can be accessed from Main and Binder thread.
    @Nullable private volatile StateCallback mUserActivityViewCallback;

    @Nullable private Car mCar;
    @Nullable private CarUxRestrictionsManager mUxRestrictionsManager;

    private int mVirtualDisplayId = Display.INVALID_DISPLAY;

    public CarActivityView(Context context) {
        this(context, /*attrs=*/ null);
    }

    public CarActivityView(Context context, AttributeSet attrs) {
        this(context, attrs, /*defStyle=*/ 0);
    }

    public CarActivityView(Context context, AttributeSet attrs, int defStyle) {
        this(context, attrs, defStyle,  /*singleTaskInstance=*/ false);
    }

    public CarActivityView(
            Context context, AttributeSet attrs, int defStyle, boolean singleTaskInstance) {
        super(context, attrs, defStyle, singleTaskInstance, /*usePublicVirtualDisplay=*/ true);
        super.setCallback(new CarActivityViewCallback());
    }

    @Override
    public void setCallback(StateCallback callback) {
        mUserActivityViewCallback = callback;
        if (getVirtualDisplayId() != Display.INVALID_DISPLAY && callback != null) {
            callback.onActivityViewReady(this);
        }
    }

    private static void reportPhysicalDisplayId(CarUxRestrictionsManager manager,
            int virtualDisplayId, int physicalDisplayId) {
        Log.d(TAG, "reportPhysicalDisplayId: virtualDisplayId=" + virtualDisplayId
                + ", physicalDisplayId=" + physicalDisplayId);
        if (virtualDisplayId == Display.INVALID_DISPLAY) {
            Log.w(TAG, "No virtual display to report");
            return;
        }
        if (manager == null) {
            Log.w(TAG, "CarUxRestrictionsManager is not ready yet");
            return;
        }
        manager.reportVirtualDisplayToPhysicalDisplay(virtualDisplayId, physicalDisplayId);
    }

    // Intercepts ActivityViewCallback and reports it's display-id changes.
    private class CarActivityViewCallback extends StateCallback {
        // onActivityViewReady() and onActivityViewDestroyed() are called in the main thread.
        @Override
        public void onActivityViewReady(ActivityView activityView) {
            // Stores the virtual display id to use it onActivityViewDestroyed().
            mVirtualDisplayId = getVirtualDisplayId();
            reportPhysicalDisplayId(
                    mUxRestrictionsManager, mVirtualDisplayId, mContext.getDisplayId());

            StateCallback stateCallback = mUserActivityViewCallback;
            if (stateCallback != null) {
                stateCallback.onActivityViewReady(activityView);
            }
        }

        @Override
        public void onActivityViewDestroyed(ActivityView activityView) {
            // getVirtualDisplayId() will return INVALID_DISPLAY inside onActivityViewDestroyed(),
            // because AV.mVirtualDisplay was already released during AV.performRelease().
            int virtualDisplayId = mVirtualDisplayId;
            mVirtualDisplayId = Display.INVALID_DISPLAY;
            reportPhysicalDisplayId(
                    mUxRestrictionsManager, virtualDisplayId, Display.INVALID_DISPLAY);

            StateCallback stateCallback = mUserActivityViewCallback;
            if (stateCallback != null) {
                stateCallback.onActivityViewDestroyed(activityView);
            }
        }

        @Override
        public void onTaskCreated(int taskId, ComponentName componentName) {
            StateCallback stateCallback = mUserActivityViewCallback;
            if (stateCallback != null) {
                stateCallback.onTaskCreated(taskId, componentName);
            }
        }

        @Override
        public void onTaskMovedToFront(int taskId) {
            StateCallback stateCallback = mUserActivityViewCallback;
            if (stateCallback != null) {
                stateCallback.onTaskMovedToFront(taskId);
            }
        }

        @Override
        public void onTaskRemovalStarted(int taskId) {
            StateCallback stateCallback = mUserActivityViewCallback;
            if (stateCallback != null) {
                stateCallback.onTaskRemovalStarted(taskId);
            }
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mCar = Car.createCar(mContext, /*handler=*/ null,
                Car.CAR_WAIT_TIMEOUT_DO_NOT_WAIT,
                (car, ready) -> {
                    // Expect to be called in the main thread, since passed a 'null' handler
                    // in Car.createCar().
                    if (!ready) return;
                    mUxRestrictionsManager = (CarUxRestrictionsManager) car.getCarManager(
                            Car.CAR_UX_RESTRICTION_SERVICE);
                    if (mVirtualDisplayId != Display.INVALID_DISPLAY) {
                        // When the CarService is reconnected, we'd like to report the physical
                        // display id again, since the previously reported mapping could be gone.
                        reportPhysicalDisplayId(
                                mUxRestrictionsManager, mVirtualDisplayId, mContext.getDisplayId());
                    }
                });
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mCar != null) mCar.disconnect();
    }
}
