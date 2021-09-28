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

package com.android.experimentalcar;

import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierConfig;
import android.car.experimental.DriverAwarenessSupplierService;
import android.car.experimental.IDriverAwarenessSupplier;
import android.car.experimental.IDriverAwarenessSupplierCallback;
import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Looper;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.Log;
import android.view.Display;
import android.view.InputChannel;
import android.view.InputEvent;
import android.view.InputEventReceiver;
import android.view.InputMonitor;
import android.view.MotionEvent;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;


/**
 * A driver awareness supplier that estimates the driver's current awareness level based on touches
 * on the headunit.
 */
public class TouchDriverAwarenessSupplier extends IDriverAwarenessSupplier.Stub {

    private static final String TAG = "Car.TouchAwarenessSupplier";
    private static final String TOUCH_INPUT_CHANNEL_NAME = "TouchDriverAwarenessInputChannel";

    private static final long MAX_STALENESS = DriverAwarenessSupplierService.NO_STALENESS;

    @VisibleForTesting
    static final float INITIAL_DRIVER_AWARENESS_VALUE = 1.0f;

    private final AtomicInteger mCurrentPermits = new AtomicInteger();
    private final ScheduledExecutorService mRefreshScheduler;
    private final Looper mLooper;
    private final Context mContext;
    private final ITimeSource mTimeSource;
    private final Runnable mRefreshPermitRunnable;
    private final IDriverAwarenessSupplierCallback mDriverAwarenessSupplierCallback;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private long mLastEventMillis;

    @GuardedBy("mLock")
    private ScheduledFuture<?> mRefreshScheduleHandle;

    @GuardedBy("mLock")
    private Config mConfig;

    // Main thread only. Hold onto reference to avoid garbage collection
    private InputMonitor mInputMonitor;

    // Main thread only. Hold onto reference to avoid garbage collection
    private InputEventReceiver mInputEventReceiver;

    TouchDriverAwarenessSupplier(Context context,
            IDriverAwarenessSupplierCallback driverAwarenessSupplierCallback, Looper looper) {
        this(context, driverAwarenessSupplierCallback, Executors.newScheduledThreadPool(1),
                looper, new SystemTimeSource());
    }

    @VisibleForTesting
    TouchDriverAwarenessSupplier(
            Context context,
            IDriverAwarenessSupplierCallback driverAwarenessSupplierCallback,
            ScheduledExecutorService refreshScheduler,
            Looper looper,
            ITimeSource timeSource) {
        mContext = context;
        mDriverAwarenessSupplierCallback = driverAwarenessSupplierCallback;
        mRefreshScheduler = refreshScheduler;
        mLooper = looper;
        mTimeSource = timeSource;
        mRefreshPermitRunnable =
                () -> {
                    synchronized (mLock) {
                        handlePermitRefreshLocked(mTimeSource.elapsedRealtime());
                    }
                };
    }


    @Override
    public void onReady() {
        try {
            mDriverAwarenessSupplierCallback.onConfigLoaded(
                    new DriverAwarenessSupplierConfig(MAX_STALENESS));
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to send config - abandoning ready process", e);
            return;
        }
        // send an initial event, as required by the IDriverAwarenessSupplierCallback spec
        try {
            mDriverAwarenessSupplierCallback.onDriverAwarenessUpdated(
                    new DriverAwarenessEvent(mTimeSource.elapsedRealtime(),
                            INITIAL_DRIVER_AWARENESS_VALUE));
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to emit initial awareness event", e);
        }
        synchronized (mLock) {
            mConfig = loadConfig();
            logd("Config loaded: " + mConfig);
            mCurrentPermits.set(mConfig.getMaxPermits());
        }
        startTouchMonitoring();
    }

    @Override
    public void setCallback(IDriverAwarenessSupplierCallback callback) {
        // no-op - the callback is initialized in the constructor
    }

    private Config loadConfig() {
        int maxPermits = mContext.getResources().getInteger(
                R.integer.driverAwarenessTouchModelMaxPermits);
        if (maxPermits <= 0) {
            throw new IllegalArgumentException("driverAwarenessTouchModelMaxPermits must be >0");
        }
        int refreshIntervalMillis = mContext.getResources().getInteger(
                R.integer.driverAwarenessTouchModelPermitRefreshIntervalMs);
        if (refreshIntervalMillis <= 0) {
            throw new IllegalArgumentException(
                    "driverAwarenessTouchModelPermitRefreshIntervalMs must be >0");
        }
        int throttleDurationMillis = mContext.getResources().getInteger(
                R.integer.driverAwarenessTouchModelThrottleMs);
        if (throttleDurationMillis <= 0) {
            throw new IllegalArgumentException("driverAwarenessTouchModelThrottleMs must be >0");
        }
        return new Config(maxPermits, refreshIntervalMillis, throttleDurationMillis);
    }

    /**
     * Starts monitoring touches.
     */
    @VisibleForTesting
    // TODO(b/146802952) handle touch monitoring on multiple displays
    void startTouchMonitoring() {
        InputManager inputManager = (InputManager) mContext.getSystemService(Context.INPUT_SERVICE);
        mInputMonitor = inputManager.monitorGestureInput(
                TOUCH_INPUT_CHANNEL_NAME,
                Display.DEFAULT_DISPLAY);
        mInputEventReceiver = new TouchReceiver(
                mInputMonitor.getInputChannel(),
                mLooper);
    }

    /**
     * Refreshes permits on the interval specified by {@code R.integer
     * .driverAwarenessTouchModelPermitRefreshIntervalMs}.
     */
    @GuardedBy("mLock")
    private void schedulePermitRefreshLocked() {
        logd("Scheduling permit refresh interval (ms): "
                + mConfig.getPermitRefreshIntervalMillis());
        mRefreshScheduleHandle = mRefreshScheduler.scheduleAtFixedRate(
                mRefreshPermitRunnable,
                mConfig.getPermitRefreshIntervalMillis(),
                mConfig.getPermitRefreshIntervalMillis(),
                TimeUnit.MILLISECONDS);
    }

    /**
     * Stops the scheduler for refreshing the number of permits.
     */
    @GuardedBy("mLock")
    private void stopPermitRefreshLocked() {
        logd("Stopping permit refresh");
        if (mRefreshScheduleHandle != null) {
            mRefreshScheduleHandle.cancel(true);
            mRefreshScheduleHandle = null;
        }
    }

    /**
     * Consume a single permit if the event should not be throttled.
     */
    @VisibleForTesting
    @GuardedBy("mLock")
    void consumePermitLocked(long timestamp) {
        long timeSinceLastEvent = timestamp - mLastEventMillis;
        boolean isEventAccepted = timeSinceLastEvent >= mConfig.getThrottleDurationMillis();
        if (!isEventAccepted) {
            logd("Ignoring consumePermit request: event throttled");
            return;
        }
        mLastEventMillis = timestamp;
        int curPermits = mCurrentPermits.updateAndGet(cur -> Math.max(0, cur - 1));
        logd("Permit consumed to: " + curPermits);

        if (mRefreshScheduleHandle == null) {
            schedulePermitRefreshLocked();
        }

        try {
            mDriverAwarenessSupplierCallback.onDriverAwarenessUpdated(
                    new DriverAwarenessEvent(timestamp,
                            (float) curPermits / mConfig.getMaxPermits()));
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to emit awareness event", e);
        }
    }

    @VisibleForTesting
    @GuardedBy("mLock")
    void handlePermitRefreshLocked(long timestamp) {
        int curPermits = mCurrentPermits.updateAndGet(
                cur -> Math.min(cur + 1, mConfig.getMaxPermits()));
        logd("Permit refreshed to: " + curPermits);
        if (curPermits == mConfig.getMaxPermits()) {
            stopPermitRefreshLocked();
        }
        try {
            mDriverAwarenessSupplierCallback.onDriverAwarenessUpdated(
                    new DriverAwarenessEvent(timestamp,
                            (float) curPermits / mConfig.getMaxPermits()));
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to emit awareness event", e);
        }
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }

    /**
     * Receiver of all touch events. This receiver filters out all events except {@link
     * MotionEvent#ACTION_UP} events.
     */
    private class TouchReceiver extends InputEventReceiver {

        /**
         * Creates an input event receiver bound to the specified input channel.
         *
         * @param inputChannel The input channel.
         * @param looper       The looper to use when invoking callbacks.
         */
        TouchReceiver(InputChannel inputChannel, Looper looper) {
            super(inputChannel, looper);
        }

        @Override
        public void onInputEvent(InputEvent event) {
            if (!(event instanceof MotionEvent)) {
                return;
            }

            MotionEvent motionEvent = (MotionEvent) event;
            if (motionEvent.getActionMasked() == MotionEvent.ACTION_UP) {
                logd("ACTION_UP touch received");
                synchronized (mLock) {
                    consumePermitLocked(SystemClock.elapsedRealtime());
                }
            }
        }
    }

    /**
     * Configuration for a {@link TouchDriverAwarenessSupplier}.
     */
    private static class Config {

        private final int mMaxPermits;
        private final int mPermitRefreshIntervalMillis;
        private final int mThrottleDurationMillis;

        /**
         * Creates an instance of {@link Config}.
         *
         * @param maxPermits                  the maximum number of permits in the user's
         *                                    attention buffer. A user's number of permits will
         *                                    never refresh to a value higher than this.
         * @param permitRefreshIntervalMillis the refresh interval in milliseconds for refreshing
         *                                    permits
         * @param throttleDurationMillis      the duration in milliseconds representing the window
         *                                    that permit consumption is ignored after an event.
         */
        private Config(
                int maxPermits,
                int permitRefreshIntervalMillis,
                int throttleDurationMillis) {
            mMaxPermits = maxPermits;
            mPermitRefreshIntervalMillis = permitRefreshIntervalMillis;
            mThrottleDurationMillis = throttleDurationMillis;
        }

        int getMaxPermits() {
            return mMaxPermits;
        }

        int getPermitRefreshIntervalMillis() {
            return mPermitRefreshIntervalMillis;
        }

        int getThrottleDurationMillis() {
            return mThrottleDurationMillis;
        }

        @Override
        public String toString() {
            return String.format(
                    "Config{mMaxPermits=%s, mPermitRefreshIntervalMillis=%s, "
                            + "mThrottleDurationMillis=%s}",
                    mMaxPermits,
                    mPermitRefreshIntervalMillis,
                    mThrottleDurationMillis);
        }
    }
}
