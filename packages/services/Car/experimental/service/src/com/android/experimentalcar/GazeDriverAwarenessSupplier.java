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

package com.android.experimentalcar;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierService;
import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.OccupantAwarenessManager;
import android.car.occupantawareness.SystemStatusEvent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.IBinder;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

/**
 * A Driver Awareness Supplier that estimates the driver's current level of awareness based on gaze
 * history.
 *
 * <p>Attention is facilitated via {@link OccupantAwarenessManager}, which is an optional component
 * and may not be available on every platform.
 */
public class GazeDriverAwarenessSupplier extends DriverAwarenessSupplierService {
    private static final String TAG = "Car.OAS.GazeAwarenessSupplier";

    /* Maximum allowable staleness before gaze data should be considered unreliable for attention
     * monitoring, in milliseconds. */
    @VisibleForTesting
    static final long MAX_STALENESS_MILLIS = 500;

    private final Object mLock = new Object();
    private final ITimeSource mTimeSource;

    @GuardedBy("mLock")
    private GazeAttentionProcessor.Configuration mConfiguration;

    @Nullable
    private Context mContext;

    @GuardedBy("mLock")
    private Car mCar;

    @GuardedBy("mLock")
    private OccupantAwarenessManager mOasManager;

    @GuardedBy("mLock")
    private GazeAttentionProcessor mProcessor;

    /**
     * Empty constructor allows system service creation.
     */
    public GazeDriverAwarenessSupplier() {
        this(/* context= */ null, new SystemTimeSource());
    }

    @VisibleForTesting
    GazeDriverAwarenessSupplier(Context context, ITimeSource timeSource) {
        mContext = context;
        mTimeSource = timeSource;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (mContext == null) {
            mContext = this;
        }
    }

    @Override
    public void onDestroy() {
        synchronized (mLock) {
            if (mCar != null && mCar.isConnected()) {
                mCar.disconnect();
            }
        }
        super.onDestroy();
    }

    /**
     * Gets the self-reported maximum allowable staleness before the supplier should be considered
     * failed, in milliseconds.
     */
    @Override
    public long getMaxStalenessMillis() {
        return MAX_STALENESS_MILLIS;
    }

    @Override
    public IBinder onBind(Intent intent) {
        logd("onBind with intent: " + intent);
        return super.onBind(intent);
    }

    @Override
    public void onReady() {
        synchronized (mLock) {
            mConfiguration = loadConfiguration();
            mProcessor = new GazeAttentionProcessor(mConfiguration);
            mCar = Car.createCar(mContext);
            if (mCar != null) {
                if (mOasManager == null && mCar.isFeatureEnabled(Car.OCCUPANT_AWARENESS_SERVICE)) {
                    mOasManager =
                            (OccupantAwarenessManager)
                                    mCar.getCarManager(Car.OCCUPANT_AWARENESS_SERVICE);

                    if (mOasManager == null) {
                        Log.e(TAG, "Failed to get OccupantAwarenessManager.");
                    }
                }

                if (mOasManager != null) {
                    logd("Registering callback with OAS manager");
                    mOasManager.registerChangeCallback(new ChangeCallback());
                }
            }
        }

        // Send an initial value once the provider is ready, as required by {link
        // IDriverAwarenessSupplierCallback}.
        emitAwarenessEvent(
                new DriverAwarenessEvent(
                        mTimeSource.elapsedRealtime(), mConfiguration.initialValue));
    }

    /**
     * Returns whether this car supports gaze based awareness.
     *
     * <p>The underlying Occupant Awareness System is optional and may not be supported on every
     * vehicle. This method must be called *after* onReady().
     */
    public boolean supportsGaze() throws IllegalStateException {
        synchronized (mLock) {
            if (mCar == null) {
                throw new IllegalStateException(
                        "Must call onReady() before querying for gaze support");
            }

            // Occupant Awareness is optional and may not be available on this machine. If not
            // available, the returned manager will be null.
            if (mOasManager == null) {
                logd("Occupant Awareness System is not available on this machine");
                return false;
            }

            int driver_capability =
                    mOasManager.getCapabilityForRole(
                            OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER);

            logd("Driver capabilities flags: " + driver_capability);

            return (driver_capability & SystemStatusEvent.DETECTION_TYPE_DRIVER_MONITORING) > 0;
        }
    }

    /** Builds {@link GazeAttentionProcessor.Configuration} using context resources. */
    private GazeAttentionProcessor.Configuration loadConfiguration() {
        Resources resources = mContext.getResources();

        return new GazeAttentionProcessor.Configuration(
                resources.getFloat(R.fraction.driverAwarenessGazeModelInitialValue),
                resources.getFloat(R.fraction.driverAwarenessGazeModelDecayRate),
                resources.getFloat(R.fraction.driverAwarenessGazeModelGrowthRate));
    }

    /** Processes a new detection event, updating the current attention value. */
    @VisibleForTesting
    void processDetectionEvent(@NonNull OccupantAwarenessDetection event) {
        if (event.role == OccupantAwarenessDetection.VEHICLE_OCCUPANT_DRIVER
                && event.gazeDetection != null) {
            float awarenessValue;
            synchronized (mLock) {
                awarenessValue =
                        mProcessor.updateAttention(event.gazeDetection, event.timestampMillis);
            }

            emitAwarenessEvent(new DriverAwarenessEvent(event.timestampMillis, awarenessValue));
        }
    }

    @VisibleForTesting
    void emitAwarenessEvent(@NonNull DriverAwarenessEvent event) {
        logd("Emitting new event: " + event);
        onDriverAwarenessUpdated(event);
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }

    /** Callback when the system status changes or a new detection is made. */
    private class ChangeCallback extends OccupantAwarenessManager.ChangeCallback {
        /** Called when the system state changes changes. */
        @Override
        public void onSystemStateChanged(@NonNull SystemStatusEvent status) {
            logd("New status: " + status);
        }

        /** Called when a detection event is generated. */
        @Override
        public void onDetectionEvent(@NonNull OccupantAwarenessDetection event) {
            logd("New detection: " + event);
            processDetectionEvent(event);
        }
    }
}
