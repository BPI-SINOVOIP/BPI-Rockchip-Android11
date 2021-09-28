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
package android.car.experimental;


import android.annotation.CallSuper;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

/**
 * The supplier for providing a stream of the driver's current situational awareness.
 *
 * <p>A perfect understanding of driver awareness requires years of extensive research and signals
 * that suggest the cognitive situational awareness of the driver. Implementations of this class
 * attempt to approximate driver awareness using concrete, but less accurate signals, such as gaze
 * or touch.
 *
 * <p>Suppliers should notify of updates to the driver awareness level through {@link
 * #onDriverAwarenessUpdated(DriverAwarenessEvent)}.
 *
 * <p>Suppliers define the amount of time until their data should be considered stale through
 * {@link #getMaxStalenessMillis()}. After that amount of time data from this supplier will no
 * longer be considered fresh. {@link #NO_STALENESS} is meant to be used by change-based suppliers
 * such as a touch supplier - it is not appropriate for data signals that change continuous over
 * time.
 *
 * <p>If this supplier has its own internal configuration, that configuration must be configurable
 * by locale.
 *
 * <p>It is the attention supplier's responsibility to make sure that it only sends high-quality
 * data events.
 */
public abstract class DriverAwarenessSupplierService extends Service {
    private static final String TAG = "DriverAwarenessSupplierService";

    /**
     * Value that can be returned by {@link #getMaxStalenessMillis()} to indicate that an attention
     * supplier sends change-events instead of push events on a regular interval. Should only be
     * used for a supplier that is guaranteed to always be running (e.g. it won't crash or have
     * periods of poor data).
     */
    public static final long NO_STALENESS = -1;

    private final Object mLock = new Object();

    private SupplierBinder mSupplierBinder;

    @GuardedBy("mLock")
    private IDriverAwarenessSupplierCallback mDriverAwarenessSupplierCallback;

    /**
     * Returns the duration in milliseconds after which the input from this supplier should be
     * considered stale. This method should return a positive value greater than 0. There is no
     * technical limit on the value returned here, but a value of 1000ms (1 second) would likely be
     * considered too high since the driving environment can change drastically in that amount of
     * time.
     *
     * <p>This can also return {@link #NO_STALENESS} if the supplier only emits change events and
     * has no risk of failing to emit those change events within a reasonable amount of time.
     *
     * <p>Data should be sent more frequently than the staleness period defined here.
     */
    public abstract long getMaxStalenessMillis();

    /**
     * The distraction service is ready to start receiving events via {@link
     * #onDriverAwarenessUpdated(DriverAwarenessEvent)}.
     */
    protected abstract void onReady();

    @Override
    @CallSuper
    public IBinder onBind(Intent intent) {
        logd("onBind, intent: " + intent);
        if (mSupplierBinder == null) {
            mSupplierBinder = new SupplierBinder();
        }
        return mSupplierBinder;
    }

    /**
     * The driver awareness has changed - emit that update to the {@link
     * com.android.experimentalcar.DriverDistractionExperimentalFeatureService}.
     */
    protected void onDriverAwarenessUpdated(DriverAwarenessEvent event) {
        logd("onDriverAwarenessUpdated: " + event);
        synchronized (mLock) {
            if (mDriverAwarenessSupplierCallback != null) {
                try {
                    mDriverAwarenessSupplierCallback.onDriverAwarenessUpdated(event);
                } catch (RemoteException e) {
                    Log.e(TAG, "Remote exception", e);
                }
            }
        }
    }

    private void handleReady() {
        synchronized (mLock) {
            try {
                mDriverAwarenessSupplierCallback.onConfigLoaded(
                        new DriverAwarenessSupplierConfig(getMaxStalenessMillis()));
            } catch (RemoteException e) {
                Log.e(TAG, "Unable to send config - abandoning ready process", e);
                return;
            }
        }
        onReady();
    }

    /**
     * The binder between this service and
     * {@link com.android.experimentalcar.DriverDistractionExperimentalFeatureService}.
     */
    @VisibleForTesting
    public class SupplierBinder extends IDriverAwarenessSupplier.Stub {

        @Override
        public void onReady() {
            handleReady();
        }

        @Override
        public void setCallback(IDriverAwarenessSupplierCallback callback) {
            synchronized (mLock) {
                mDriverAwarenessSupplierCallback = callback;
            }
        }
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }
}
