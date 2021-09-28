/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.car.content.pm;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.SystemApi;
import android.annotation.TestApi;
import android.app.PendingIntent;
import android.car.Car;
import android.car.CarManagerBase;
import android.content.ComponentName;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides car specific API related with package management.
 */
public final class CarPackageManager extends CarManagerBase {
    private static final String TAG = "CarPackageManager";

    /**
     * Flag for {@link #setAppBlockingPolicy(String, CarAppBlockingPolicy, int)}. When this
     * flag is set, the call will be blocked until policy is set to system. This can take time
     * and the flag cannot be used in main thread.
     * @hide
     */
    @SystemApi
    public static final int FLAG_SET_POLICY_WAIT_FOR_CHANGE = 0x1;
    /**
     * Flag for {@link #setAppBlockingPolicy(String, CarAppBlockingPolicy, int)}. When this
     * flag is set, passed policy is added to existing policy set from the current package.
     * If none of {@link #FLAG_SET_POLICY_ADD} or {@link #FLAG_SET_POLICY_REMOVE} is set, existing
     * policy is replaced. Note that policy per each package is always replaced and will not be
     * added.
     * @hide
     */
    @SystemApi
    public static final int FLAG_SET_POLICY_ADD = 0x2;
    /**
     * Flag for {@link #setAppBlockingPolicy(String, CarAppBlockingPolicy, int)}. When this
     * flag is set, passed policy is removed from existing policy set from the current package.
     * If none of {@link #FLAG_SET_POLICY_ADD} or {@link #FLAG_SET_POLICY_REMOVE} is set, existing
     * policy is replaced.
     * @hide
     */
    @SystemApi
    public static final int FLAG_SET_POLICY_REMOVE = 0x4;

    /** @hide */
    @IntDef(flag = true,
            value = {FLAG_SET_POLICY_WAIT_FOR_CHANGE, FLAG_SET_POLICY_ADD, FLAG_SET_POLICY_REMOVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SetPolicyFlags {}

    private final ICarPackageManager mService;

    /** @hide */
    public CarPackageManager(Car car, IBinder service) {
        super(car);
        mService = ICarPackageManager.Stub.asInterface(service);
    }

    /** @hide */
    @Override
    public void onCarDisconnected() {
        // nothing to do
    }

    /**
     * Set Application blocking policy for system app. {@link #FLAG_SET_POLICY_ADD} or
     * {@link #FLAG_SET_POLICY_REMOVE} flag allows adding or removing from already set policy. When
     * none of these flags are set, it will completely replace existing policy for each package
     * specified.
     * When {@link #FLAG_SET_POLICY_WAIT_FOR_CHANGE} flag is set, this call will be blocked
     * until the policy is set to system and become effective. Otherwise, the call will start
     * changing the policy but it will be completed asynchronously and the call will return
     * without waiting for system level policy change.
     *
     * @param packageName Package name of the client. If wrong package name is passed, exception
     *        will be thrown. This name is used to update the policy.
     * @param policy
     * @param flags
     * @throws SecurityException if caller has no permission.
     * @throws IllegalArgumentException For wrong or invalid arguments.
     * @throws IllegalStateException If {@link #FLAG_SET_POLICY_WAIT_FOR_CHANGE} is set while
     *         called from main thread.
     * @hide
     */
    @SystemApi
    public void setAppBlockingPolicy(
            String packageName, CarAppBlockingPolicy policy, @SetPolicyFlags int flags) {
        if ((flags & FLAG_SET_POLICY_WAIT_FOR_CHANGE) != 0
                && Looper.getMainLooper().isCurrentThread()) {
            throw new IllegalStateException(
                    "FLAG_SET_POLICY_WAIT_FOR_CHANGE cannot be used in main thread");
        }
        try {
            mService.setAppBlockingPolicy(packageName, policy, flags);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Restarts the requested task. If task with {@code taskId} does not exist, do nothing.
     *
     * <p>This requires {@code android.permission.REAL_GET_TASKS} permission.
     *
     * @hide
     */
    public void restartTask(int taskId) {
        try {
            mService.restartTask(taskId);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Check if finishing Activity will lead into safe Activity (=allowed Activity) to be shown.
     * This can be used by unsafe activity blocking Activity to check if finishing itself can
     * lead into being launched again due to unsafe activity shown. Note that checking this does not
     * guarantee that blocking will not be done as driving state can change after this call is made.
     *
     * @param activityName
     * @return true if there is a safe Activity (or car is stopped) in the back of task stack
     *         so that finishing the Activity will not trigger another Activity blocking. If
     *         the given Activity is not in foreground, then it will return true as well as
     *         finishing the Activity will not make any difference.
     *
     * @hide
     */
    @SystemApi
    public boolean isActivityBackedBySafeActivity(ComponentName activityName) {
        try {
            return mService.isActivityBackedBySafeActivity(activityName);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Enable/Disable Activity Blocking.  This is to provide an option for toggling app blocking
     * behavior for development purposes.
     * @hide
     */
    @TestApi
    public void setEnableActivityBlocking(boolean enable) {
        try {
            mService.setEnableActivityBlocking(enable);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Returns whether an activity is distraction optimized, i.e, allowed in a restricted
     * driving state.
     *
     * @param packageName the activity's {@link android.content.pm.ActivityInfo#packageName}.
     * @param className the activity's {@link android.content.pm.ActivityInfo#name}.
     * @return true if the activity is distraction optimized, false if it isn't or if the value
     *         could not be determined.
     */
    public boolean isActivityDistractionOptimized(String packageName, String className) {
        try {
            return mService.isActivityDistractionOptimized(packageName, className);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns whether the given {@link PendingIntent} represents an activity that is distraction
     * optimized, i.e, allowed in a restricted driving state.
     *
     * @param pendingIntent the {@link PendingIntent} to check.
     * @return true if the pending intent represents an activity that is distraction optimized,
     *         false if it isn't or if the value could not be determined.
     */
    public boolean isPendingIntentDistractionOptimized(@NonNull PendingIntent pendingIntent) {
        try {
            return mService.isPendingIntentDistractionOptimized(pendingIntent);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Check if given service is distraction optimized, i.e, allowed in a restricted
     * driving state.
     *
     * @param packageName
     * @param className
     * @return
     */
    public boolean isServiceDistractionOptimized(String packageName, String className) {
        try {
            return mService.isServiceDistractionOptimized(packageName, className);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }
}
