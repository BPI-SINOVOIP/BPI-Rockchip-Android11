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
package com.android.compatibility.common.util;

import android.app.Activity;
import android.app.Application.ActivityLifecycleCallbacks;
import android.os.Bundle;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.NonNull;

import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper object used to watch for activities lifecycle events.
 *
 * <p><b>NOTE:</b> currently it's limited to just one occurrence of each event.
 *
 * <p>These limitations will be fixed as needed (A.K.A. K.I.S.S. :-)
 */
public final class ActivitiesWatcher implements ActivityLifecycleCallbacks {

    private static final String TAG = ActivitiesWatcher.class.getSimpleName();

    private final Map<String, ActivityWatcher> mWatchers = new ArrayMap<>();
    private final long mTimeoutMs;

    /**
     * Default constructor.
     *
     * @param timeoutMs how long to wait for given lifecycle event before timing out.
     */
    public ActivitiesWatcher(long timeoutMs) {
        mTimeoutMs = timeoutMs;
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        Log.v(TAG, "onActivityCreated(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.CREATED);
    }

    @Override
    public void onActivityStarted(Activity activity) {
        Log.v(TAG, "onActivityStarted(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.STARTED);
    }

    @Override
    public void onActivityResumed(Activity activity) {
        Log.v(TAG, "onActivityResumed(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.RESUMED);
    }

    @Override
    public void onActivityPaused(Activity activity) {
        Log.v(TAG, "onActivityPaused(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.PAUSED);
    }

    @Override
    public void onActivityStopped(Activity activity) {
        Log.v(TAG, "onActivityStopped(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.STOPPED);
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
        Log.v(TAG, "onActivitySaveInstanceState(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.SAVE_INSTANCE);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        Log.v(TAG, "onActivityDestroyed(): " + activity);
        notifyWatcher(activity, ActivityLifecycle.DESTROYED);
    }

    /**
     * Gets a watcher for the given activity.
     *
     * @throws IllegalStateException if already registered.
     */
    public ActivityWatcher watch(@NonNull Class<? extends Activity> clazz) {
        return watch(clazz.getName());
    }

    @Override
    public String toString() {
        return "[ActivitiesWatcher: activities=" + mWatchers.keySet() + "]";
    }

    /**
     * Gets a watcher for the given activity.
     *
     * @throws IllegalStateException if already registered.
     */
    public ActivityWatcher watch(@NonNull String className) {
        if (mWatchers.containsKey(className)) {
            throw new IllegalStateException("Already watching " + className);
        }
        Log.d(TAG, "Registering watcher for " + className);
        final ActivityWatcher watcher = new ActivityWatcher(mTimeoutMs);
        mWatchers.put(className,  watcher);
        return watcher;
    }

    private void notifyWatcher(@NonNull Activity activity, @NonNull ActivityLifecycle lifecycle) {
        final String className = activity.getComponentName().getClassName();
        final ActivityWatcher watcher = mWatchers.get(className);
        if (watcher != null) {
            Log.d(TAG, "notifying watcher of " + className + " of " + lifecycle);
            watcher.notify(lifecycle);
        } else {
            Log.v(TAG, lifecycle + ": no watcher for " + className);
        }
    }

    /**
     * Object used to watch for acitivity lifecycle events.
     *
     * <p><b>NOTE: </b>currently it only supports one occurrence for each event.
     */
    public static final class ActivityWatcher {
        private final CountDownLatch mCreatedLatch = new CountDownLatch(1);
        private final CountDownLatch mStartedLatch = new CountDownLatch(1);
        private final CountDownLatch mResumedLatch = new CountDownLatch(1);
        private final CountDownLatch mPausedLatch = new CountDownLatch(1);
        private final CountDownLatch mStoppedLatch = new CountDownLatch(1);
        private final CountDownLatch mSaveInstanceLatch = new CountDownLatch(1);
        private final CountDownLatch mDestroyedLatch = new CountDownLatch(1);
        private final long mTimeoutMs;

        private ActivityWatcher(long timeoutMs) {
            mTimeoutMs = timeoutMs;
        }

        /**
         * Blocks until the given lifecycle event happens.
         *
         * @throws IllegalStateException if it times out while waiting.
         * @throws InterruptedException if interrupted while waiting.
         */
        public void waitFor(@NonNull ActivityLifecycle lifecycle) throws InterruptedException {
            final CountDownLatch latch = getLatch(lifecycle);
            final boolean called = latch.await(mTimeoutMs, TimeUnit.MILLISECONDS);
            if (!called) {
                throw new IllegalStateException(lifecycle + " not called in " + mTimeoutMs + " ms");
            }
        }

        private CountDownLatch getLatch(@NonNull ActivityLifecycle lifecycle) {
            switch (lifecycle) {
                case CREATED:
                    return mCreatedLatch;
                case STARTED:
                    return mStartedLatch;
                case RESUMED:
                    return mResumedLatch;
                case PAUSED:
                    return mPausedLatch;
                case STOPPED:
                    return mStoppedLatch;
                case SAVE_INSTANCE:
                    return mSaveInstanceLatch;
                case DESTROYED:
                    return mDestroyedLatch;
                default:
                    throw new IllegalArgumentException("unsupported lifecycle: " + lifecycle);
            }
        }

        private void notify(@NonNull ActivityLifecycle lifecycle) {
            getLatch(lifecycle).countDown();
        }
    }

    /**
     * Supported activity lifecycle.
     */
    public enum ActivityLifecycle {
        CREATED,
        STARTED,
        RESUMED,
        PAUSED,
        STOPPED,
        SAVE_INSTANCE,
        DESTROYED
    }
}
