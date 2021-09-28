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

import android.annotation.FloatRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.VehiclePropertyIds;
import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierConfig;
import android.car.experimental.DriverAwarenessSupplierService;
import android.car.experimental.DriverDistractionChangeEvent;
import android.car.experimental.ExperimentalCar;
import android.car.experimental.IDriverAwarenessSupplier;
import android.car.experimental.IDriverAwarenessSupplierCallback;
import android.car.experimental.IDriverDistractionChangeListener;
import android.car.experimental.IDriverDistractionManager;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyEvent;
import android.car.hardware.property.CarPropertyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;
import android.util.Pair;

import com.android.car.CarServiceBase;
import com.android.car.Utils;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TimerTask;

/**
 * Driver Distraction Service for using the driver's awareness, the required awareness of the
 * driving environment to expose APIs for the driver's current distraction level.
 *
 * <p>Allows the registration of multiple {@link IDriverAwarenessSupplier} so that higher accuracy
 * signals can be used when possible, with a fallback to less accurate signals. The {@link
 * TouchDriverAwarenessSupplier} is always set to the fallback implementation - it is configured
 * to send change-events, so its data will not become stale.
 */
public final class DriverDistractionExperimentalFeatureService extends
        IDriverDistractionManager.Stub implements CarServiceBase {

    private static final String TAG = "CAR.DriverDistractionService";

    /**
     * The minimum delay between dispatched distraction events, in milliseconds.
     */
    @VisibleForTesting
    static final long DISPATCH_THROTTLE_MS = 50L;
    private static final float DEFAULT_AWARENESS_VALUE_FOR_LOG = 1.0f;
    private static final float MOVING_REQUIRED_AWARENESS = 1.0f;
    private static final float STATIONARY_REQUIRED_AWARENESS = 0.0f;
    private static final int MAX_EVENT_LOG_COUNT = 50;
    private static final int PROPERTY_UPDATE_RATE_HZ = 5;
    @VisibleForTesting
    static final float DEFAULT_AWARENESS_PERCENTAGE = 1.0f;

    private final HandlerThread mClientDispatchHandlerThread;
    private final Handler mClientDispatchHandler;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final ArrayDeque<Utils.TransitionLog> mTransitionLogs = new ArrayDeque<>();

    /**
     * All the active service connections.
     */
    @GuardedBy("mLock")
    private final List<ServiceConnection> mServiceConnections = new ArrayList<>();

    /**
     * The binder for each supplier.
     */
    @GuardedBy("mLock")
    private final Map<ComponentName, IDriverAwarenessSupplier> mSupplierBinders = new HashMap<>();

    /**
     * The configuration for each supplier.
     */
    @GuardedBy("mLock")
    private final Map<IDriverAwarenessSupplier, DriverAwarenessSupplierConfig> mSupplierConfigs =
            new HashMap<>();

    /**
     * List of driver awareness suppliers that can be used to understand the current driver
     * awareness level. Ordered from highest to lowest priority.
     */
    @GuardedBy("mLock")
    private final List<IDriverAwarenessSupplier> mPrioritizedDriverAwarenessSuppliers =
            new ArrayList<>();

    /**
     * Helper map for looking up the priority rank of a supplier by name. A higher integer value
     * represents a higher priority.
     */
    @GuardedBy("mLock")
    private final Map<IDriverAwarenessSupplier, Integer> mDriverAwarenessSupplierPriorities =
            new HashMap<>();

    /**
     * List of clients listening to UX restriction events.
     */
    private final RemoteCallbackList<IDriverDistractionChangeListener> mDistractionClients =
            new RemoteCallbackList<>();

    /**
     * Comparator used to sort {@link #mDriverAwarenessSupplierPriorities}.
     */
    private final Comparator<IDriverAwarenessSupplier> mPrioritizedSuppliersComparator =
            (left, right) -> {
                int leftPri = mDriverAwarenessSupplierPriorities.get(left);
                int rightPri = mDriverAwarenessSupplierPriorities.get(right);
                // sort descending
                return rightPri - leftPri;
            };

    /**
     * Keep track of the most recent awareness event for each supplier for use when the data from
     * higher priority suppliers becomes stale. This is necessary in order to seamlessly handle
     * fallback scenarios when data from preferred providers becomes stale.
     */
    @GuardedBy("mLock")
    private final Map<IDriverAwarenessSupplier, DriverAwarenessEventWrapper>
            mCurrentAwarenessEventsMap =
            new HashMap<>();

    /**
     * The awareness event that is currently being used to determine the driver awareness level.
     *
     * <p>This is null until it is set by the first awareness supplier to send an event
     */
    @GuardedBy("mLock")
    @Nullable
    private DriverAwarenessEventWrapper mCurrentDriverAwareness;

    /**
     * Timer to alert when the current driver awareness event has become stale.
     */
    @GuardedBy("mLock")
    private ITimer mExpiredDriverAwarenessTimer;

    /**
     * The current, non-stale, driver distraction event. Defaults to 100% awareness.
     */
    @GuardedBy("mLock")
    private DriverDistractionChangeEvent mCurrentDistractionEvent;

    /**
     * The required driver awareness based on the current driving environment, where 1.0 means that
     * full awareness is required and 0.0 means than no awareness is required.
     */
    @FloatRange(from = 0.0f, to = 1.0f)
    @GuardedBy("mLock")
    private float mRequiredAwareness = STATIONARY_REQUIRED_AWARENESS;

    @GuardedBy("mLock")
    private Car mCar;

    @GuardedBy("mLock")
    private CarPropertyManager mPropertyManager;

    /**
     * The time that last event was emitted, measured in milliseconds since boot using the {@link
     * android.os.SystemClock#uptimeMillis()} time-base.
     */
    @GuardedBy("mLock")
    private long mLastDispatchUptimeMillis;

    /**
     * Whether there is currently a pending dispatch to clients.
     */
    @GuardedBy("mLock")
    private boolean mIsDispatchQueued;

    private final Context mContext;
    private final ITimeSource mTimeSource;
    private final Looper mLooper;

    private final Runnable mDispatchCurrentDistractionRunnable = () -> {
        synchronized (mLock) {
            // dispatch whatever the current value is at this time in the future
            dispatchCurrentDistractionEventToClientsLocked(
                    mCurrentDistractionEvent);
            mIsDispatchQueued = false;
        }
    };

    /**
     * Create an instance of {@link DriverDistractionExperimentalFeatureService}.
     *
     * @param context the context
     */
    DriverDistractionExperimentalFeatureService(Context context) {
        this(context, new SystemTimeSource(), new SystemTimer(), Looper.myLooper(), null);
    }

    @VisibleForTesting
    DriverDistractionExperimentalFeatureService(
            Context context,
            ITimeSource timeSource,
            ITimer timer,
            Looper looper,
            Handler clientDispatchHandler) {
        mContext = context;
        mTimeSource = timeSource;
        mExpiredDriverAwarenessTimer = timer;
        mCurrentDistractionEvent = new DriverDistractionChangeEvent.Builder()
                .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                .build();
        mClientDispatchHandlerThread = new HandlerThread(TAG);
        mClientDispatchHandlerThread.start();
        if (clientDispatchHandler == null) {
            mClientDispatchHandler = new Handler(mClientDispatchHandlerThread.getLooper());
        } else {
            mClientDispatchHandler = clientDispatchHandler;
        }
        mLooper = looper;
    }

    @Override
    public void init() {
        // The touch supplier is an internal implementation, so it can be started initiated by its
        // constructor, unlike other suppliers
        ComponentName touchComponent = new ComponentName(mContext,
                TouchDriverAwarenessSupplier.class);
        TouchDriverAwarenessSupplier touchSupplier = new TouchDriverAwarenessSupplier(mContext,
                new DriverAwarenessSupplierCallback(touchComponent), mLooper);
        addDriverAwarenessSupplier(touchComponent, touchSupplier, /* priority= */ 0);
        touchSupplier.onReady();

        String[] preferredDriverAwarenessSuppliers = mContext.getResources().getStringArray(
                R.array.preferredDriverAwarenessSuppliers);
        for (int i = 0; i < preferredDriverAwarenessSuppliers.length; i++) {
            String supplierStringName = preferredDriverAwarenessSuppliers[i];
            ComponentName externalComponent = ComponentName.unflattenFromString(supplierStringName);
            // the touch supplier has priority 0 and preferred suppliers are higher based on order
            int priority = i + 1;
            bindDriverAwarenessSupplierService(externalComponent, priority);
        }

        synchronized (mLock) {
            mCar = Car.createCar(mContext);
            if (mCar != null) {
                mPropertyManager = (CarPropertyManager) mCar.getCarManager(Car.PROPERTY_SERVICE);
            } else {
                Log.e(TAG, "Unable to connect to car in init");
            }
        }

        if (mPropertyManager != null) {
            mPropertyManager.registerCallback(mSpeedPropertyEventCallback,
                    VehiclePropertyIds.PERF_VEHICLE_SPEED,
                    PROPERTY_UPDATE_RATE_HZ);
        } else {
            Log.e(TAG, "Unable to get car property service.");
        }
    }

    @Override
    public void release() {
        logd("release");
        mDistractionClients.kill();
        synchronized (mLock) {
            mExpiredDriverAwarenessTimer.cancel();
            mClientDispatchHandler.removeCallbacksAndMessages(null);
            for (ServiceConnection serviceConnection : mServiceConnections) {
                mContext.unbindService(serviceConnection);
            }
            if (mPropertyManager != null) {
                mPropertyManager.unregisterCallback(mSpeedPropertyEventCallback);
            }
            if (mCar != null) {
                mCar.disconnect();
            }
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*DriverDistractionExperimentalFeatureService*");
        mDistractionClients.dump(writer, "Distraction Clients ");
        writer.println("Prioritized Driver Awareness Suppliers (highest to lowest priority):");
        synchronized (mLock) {
            for (int i = 0; i < mPrioritizedDriverAwarenessSuppliers.size(); i++) {
                writer.println(
                        String.format("  %d: %s", i, mPrioritizedDriverAwarenessSuppliers.get(
                                i).getClass().getName()));
            }
            writer.println("Current Driver Awareness:");
            writer.println("  Value: "
                    + (mCurrentDriverAwareness == null ? "unknown"
                    : mCurrentDriverAwareness.mAwarenessEvent.getAwarenessValue()));
            writer.println("  Supplier: " + (mCurrentDriverAwareness == null ? "unknown"
                    : mCurrentDriverAwareness.mSupplier.getClass().getSimpleName()));
            writer.println("  Timestamp (ms since boot): "
                    + (mCurrentDriverAwareness == null ? "unknown"
                    : mCurrentDriverAwareness.mAwarenessEvent.getTimeStamp()));
            writer.println("Current Required Awareness: " + mRequiredAwareness);
            writer.println("Last Distraction Event:");
            writer.println("  Value: "
                    + (mCurrentDistractionEvent == null ? "unknown"
                    : mCurrentDistractionEvent.getAwarenessPercentage()));
            writer.println("  Timestamp (ms since boot): "
                    + (mCurrentDistractionEvent == null ? "unknown"
                    : mCurrentDistractionEvent.getElapsedRealtimeTimestamp()));
            writer.println("Dispatch Status:");
            writer.println("  mLastDispatchUptimeMillis: " + mLastDispatchUptimeMillis);
            writer.println("  mIsDispatchQueued: " + mIsDispatchQueued);
            writer.println("Change log:");
            for (Utils.TransitionLog log : mTransitionLogs) {
                writer.println(log);
            }
        }
    }

    /**
     * Bind to a {@link DriverAwarenessSupplierService} by its component name.
     *
     * @param componentName the name of the {@link DriverAwarenessSupplierService} to bind to.
     * @param priority      the priority rank of this supplier
     */
    private void bindDriverAwarenessSupplierService(ComponentName componentName, int priority) {
        Intent intent = new Intent();
        intent.setComponent(componentName);
        ServiceConnection connection = new DriverAwarenessServiceConnection(priority);
        synchronized (mLock) {
            mServiceConnections.add(connection);
        }
        if (!mContext.bindServiceAsUser(intent, connection,
                Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT, UserHandle.SYSTEM)) {
            Log.e(TAG, "Unable to bind with intent: " + intent);
            // TODO(b/146471650) attempt to rebind
        }
    }

    @VisibleForTesting
    void handleDriverAwarenessEvent(DriverAwarenessEventWrapper awarenessEventWrapper) {
        synchronized (mLock) {
            handleDriverAwarenessEventLocked(awarenessEventWrapper);
        }
    }

    /**
     * Handle the driver awareness event by:
     * <ul>
     *     <li>Cache the driver awareness event for its supplier</li>
     *     <li>Update the current awareness value</li>
     *     <li>Register to refresh the awareness value again when the new current expires</li>
     * </ul>
     *
     * @param awarenessEventWrapper the driver awareness event that has occurred
     */
    @GuardedBy("mLock")
    private void handleDriverAwarenessEventLocked(
            DriverAwarenessEventWrapper awarenessEventWrapper) {
        // update the current awareness event for the supplier, checking that it is the newest event
        IDriverAwarenessSupplier supplier = awarenessEventWrapper.mSupplier;
        long timestamp = awarenessEventWrapper.mAwarenessEvent.getTimeStamp();
        if (!mCurrentAwarenessEventsMap.containsKey(supplier)
                || mCurrentAwarenessEventsMap.get(supplier).mAwarenessEvent.getTimeStamp()
                < timestamp) {
            mCurrentAwarenessEventsMap.put(awarenessEventWrapper.mSupplier, awarenessEventWrapper);
        }

        int oldSupplierPriority = mDriverAwarenessSupplierPriorities.get(supplier);
        float oldAwarenessValue = DEFAULT_AWARENESS_VALUE_FOR_LOG;
        if (mCurrentDriverAwareness != null) {
            oldAwarenessValue = mCurrentDriverAwareness.mAwarenessEvent.getAwarenessValue();
        }

        updateCurrentAwarenessValueLocked();

        int newSupplierPriority = mDriverAwarenessSupplierPriorities.get(
                mCurrentDriverAwareness.mSupplier);
        if (mSupplierConfigs.get(mCurrentDriverAwareness.mSupplier).getMaxStalenessMillis()
                != DriverAwarenessSupplierService.NO_STALENESS
                && newSupplierPriority >= oldSupplierPriority) {
            // only reschedule an expiration if this is for a supplier that is the same or higher
            // priority than the old value. If there is a higher priority supplier with non-stale
            // data, then mCurrentDriverAwareness won't change even though we received a new event.
            scheduleExpirationTimerLocked();
        }

        if (oldAwarenessValue != mCurrentDriverAwareness.mAwarenessEvent.getAwarenessValue()) {
            logd("Driver awareness updated: "
                    + mCurrentDriverAwareness.mAwarenessEvent.getAwarenessValue());
            addTransitionLogLocked(oldAwarenessValue,
                    awarenessEventWrapper.mAwarenessEvent.getAwarenessValue(),
                    "Driver awareness updated by "
                            + awarenessEventWrapper.mSupplier.getClass().getSimpleName());
        }

        updateCurrentDistractionEventLocked();
    }

    /**
     * Get the current awareness value.
     */
    @VisibleForTesting
    DriverAwarenessEventWrapper getCurrentDriverAwareness() {
        return mCurrentDriverAwareness;
    }

    /**
     * Set the drier awareness suppliers. Allows circumventing the {@link #init()} logic.
     */
    @VisibleForTesting
    void setDriverAwarenessSuppliers(
            List<Pair<IDriverAwarenessSupplier, DriverAwarenessSupplierConfig>> suppliers) {
        mPrioritizedDriverAwarenessSuppliers.clear();
        mDriverAwarenessSupplierPriorities.clear();
        for (int i = 0; i < suppliers.size(); i++) {
            Pair<IDriverAwarenessSupplier, DriverAwarenessSupplierConfig> pair = suppliers.get(i);
            mSupplierConfigs.put(pair.first, pair.second);
            mDriverAwarenessSupplierPriorities.put(pair.first, i);
            mPrioritizedDriverAwarenessSuppliers.add(pair.first);
        }
        mPrioritizedDriverAwarenessSuppliers.sort(mPrioritizedSuppliersComparator);
    }

    /**
     * {@link CarPropertyEvent} listener registered with the {@link CarPropertyManager} for getting
     * speed change notifications.
     */
    private final CarPropertyManager.CarPropertyEventCallback mSpeedPropertyEventCallback =
            new CarPropertyManager.CarPropertyEventCallback() {
                @Override
                public void onChangeEvent(CarPropertyValue value) {
                    synchronized (mLock) {
                        handleSpeedEventLocked(value);
                    }
                }

                @Override
                public void onErrorEvent(int propId, int zone) {
                    Log.e(TAG, "Error in callback for vehicle speed");
                }
            };


    @VisibleForTesting
    @GuardedBy("mLock")
    void handleSpeedEventLocked(@NonNull CarPropertyValue value) {
        if (value.getPropertyId() != VehiclePropertyIds.PERF_VEHICLE_SPEED) {
            Log.e(TAG, "Unexpected property id: " + value.getPropertyId());
            return;
        }

        float oldValue = mRequiredAwareness;
        if ((Float) value.getValue() > 0) {
            mRequiredAwareness = MOVING_REQUIRED_AWARENESS;
        } else {
            mRequiredAwareness = STATIONARY_REQUIRED_AWARENESS;
        }

        if (Float.compare(oldValue, mRequiredAwareness) != 0) {
            logd("Required awareness updated: " + mRequiredAwareness);
            addTransitionLogLocked(oldValue, mRequiredAwareness, "Required awareness");
            updateCurrentDistractionEventLocked();
        }
    }

    @GuardedBy("mLock")
    private void updateCurrentDistractionEventLocked() {
        if (mCurrentDriverAwareness == null) {
            logd("Driver awareness level is not yet known");
            return;
        }
        float awarenessPercentage;
        if (mRequiredAwareness == 0) {
            // avoid divide by 0 error - awareness percentage should be 100% when required
            // awareness is 0
            awarenessPercentage = 1.0f;
        } else {
            // Cap awareness percentage at 100%
            awarenessPercentage = Math.min(
                    mCurrentDriverAwareness.mAwarenessEvent.getAwarenessValue()
                            / mRequiredAwareness, 1.0f);
        }
        if (Float.compare(mCurrentDistractionEvent.getAwarenessPercentage(), awarenessPercentage)
                == 0) {
            // no need to dispatch unless there's a change
            return;
        }

        addTransitionLogLocked(mCurrentDistractionEvent.getAwarenessPercentage(),
                awarenessPercentage, "Awareness percentage");

        mCurrentDistractionEvent = new DriverDistractionChangeEvent.Builder()
                .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                .setAwarenessPercentage(awarenessPercentage)
                .build();

        long nowUptimeMillis = mTimeSource.uptimeMillis();
        if (shouldThrottleDispatchEventLocked(nowUptimeMillis)) {
            scheduleAwarenessDispatchLocked(nowUptimeMillis);
        } else {
            // if event doesn't need to be throttled, emit immediately
            DriverDistractionChangeEvent changeEvent = mCurrentDistractionEvent;
            mClientDispatchHandler.post(
                    () -> dispatchCurrentDistractionEventToClientsLocked(changeEvent));
        }
    }

    @GuardedBy("mLock")
    private void scheduleAwarenessDispatchLocked(long uptimeMillis) {
        if (mIsDispatchQueued) {
            logd("Dispatch event is throttled and already scheduled.");
            return;
        }

        // schedule a dispatch for when throttle window has passed
        long delayMs = mLastDispatchUptimeMillis + DISPATCH_THROTTLE_MS - uptimeMillis;
        if (delayMs < 0) {
            Log.e(TAG, String.format(
                    "Delay for (%s) calculated to be negative (%s), so dispatching immediately",
                    mCurrentDistractionEvent, delayMs));
            delayMs = 0;
        }
        logd(String.format("Dispatch event (%s) is throttled. Scheduled to emit in %sms",
                mCurrentDistractionEvent, delayMs));
        mIsDispatchQueued = true;
        mClientDispatchHandler.postDelayed(mDispatchCurrentDistractionRunnable, delayMs);
    }

    @GuardedBy("mLock")
    private boolean shouldThrottleDispatchEventLocked(long uptimeMillis) {
        return uptimeMillis < mLastDispatchUptimeMillis + DISPATCH_THROTTLE_MS;
    }

    @GuardedBy("mLock")
    private void dispatchCurrentDistractionEventToClientsLocked(
            DriverDistractionChangeEvent changeEvent) {
        mLastDispatchUptimeMillis = mTimeSource.uptimeMillis();
        logd("Dispatching event to clients: " + changeEvent);
        int numClients = mDistractionClients.beginBroadcast();
        for (int i = 0; i < numClients; i++) {
            IDriverDistractionChangeListener callback = mDistractionClients.getBroadcastItem(i);
            try {
                callback.onDriverDistractionChange(changeEvent);
            } catch (RemoteException ignores) {
                // ignore
            }
        }
        mDistractionClients.finishBroadcast();
    }

    /**
     * Internally register the supplier with the specified priority.
     */
    private void addDriverAwarenessSupplier(
            ComponentName componentName,
            IDriverAwarenessSupplier awarenessSupplier,
            int priority) {
        synchronized (mLock) {
            mSupplierBinders.put(componentName, awarenessSupplier);
            mDriverAwarenessSupplierPriorities.put(awarenessSupplier, priority);
            mPrioritizedDriverAwarenessSuppliers.add(awarenessSupplier);
            mPrioritizedDriverAwarenessSuppliers.sort(mPrioritizedSuppliersComparator);
        }
    }

    /**
     * Remove references to a supplier.
     */
    private void removeDriverAwarenessSupplier(ComponentName componentName) {
        synchronized (mLock) {
            IDriverAwarenessSupplier supplier = mSupplierBinders.get(componentName);
            mSupplierBinders.remove(componentName);
            mDriverAwarenessSupplierPriorities.remove(supplier);
            mPrioritizedDriverAwarenessSuppliers.remove(supplier);
        }
    }

    /**
     * Update {@link #mCurrentDriverAwareness} based on the current driver awareness events for each
     * supplier.
     */
    @GuardedBy("mLock")
    private void updateCurrentAwarenessValueLocked() {
        for (IDriverAwarenessSupplier supplier : mPrioritizedDriverAwarenessSuppliers) {
            long supplierMaxStaleness = mSupplierConfigs.get(supplier).getMaxStalenessMillis();
            DriverAwarenessEventWrapper eventForSupplier = mCurrentAwarenessEventsMap.get(supplier);
            if (eventForSupplier == null) {
                continue;
            }
            if (supplierMaxStaleness == DriverAwarenessSupplierService.NO_STALENESS) {
                // this supplier can't be stale, so use its information
                mCurrentDriverAwareness = eventForSupplier;
                return;
            }

            long oldestFreshTimestamp = mTimeSource.elapsedRealtime() - supplierMaxStaleness;
            if (eventForSupplier.mAwarenessEvent.getTimeStamp() > oldestFreshTimestamp) {
                // value is still fresh, so use it
                mCurrentDriverAwareness = eventForSupplier;
                return;
            }
        }

        if (mCurrentDriverAwareness == null) {
            // There must always at least be a fallback supplier with NO_STALENESS configuration.
            // Since we control this configuration, getting this exception represents a developer
            // error in initialization.
            throw new IllegalStateException(
                    "Unable to determine the current driver awareness value");
        }
    }

    /**
     * Sets a timer to update the refresh the awareness value once the current value has become
     * stale.
     */
    @GuardedBy("mLock")
    private void scheduleExpirationTimerLocked() {
        // reschedule the current awareness expiration task
        mExpiredDriverAwarenessTimer.reset();
        long delay = mCurrentDriverAwareness.mAwarenessEvent.getTimeStamp()
                - mTimeSource.elapsedRealtime()
                + mCurrentDriverAwareness.mMaxStaleness;
        if (delay < 0) {
            // somehow the event is already stale
            synchronized (mLock) {
                updateCurrentAwarenessValueLocked();
            }
            return;
        }
        mExpiredDriverAwarenessTimer.schedule(new TimerTask() {
            @Override
            public void run() {
                logd("Driver awareness has become stale. Selecting new awareness level.");
                synchronized (mLock) {
                    updateCurrentAwarenessValueLocked();
                    updateCurrentDistractionEventLocked();
                }
            }
        }, delay);

        logd(String.format(
                "Current awareness value is stale after %sms and is scheduled to expire in %sms",
                mCurrentDriverAwareness.mMaxStaleness, delay));
    }

    /**
     * Add the state change to the transition log.
     *
     * @param oldValue the old value
     * @param newValue the new value
     * @param extra    name of the value being changed
     */
    @GuardedBy("mLock")
    private void addTransitionLogLocked(float oldValue, float newValue, String extra) {
        if (mTransitionLogs.size() >= MAX_EVENT_LOG_COUNT) {
            mTransitionLogs.remove();
        }

        Utils.TransitionLog tLog = new Utils.TransitionLog(TAG, oldValue, newValue,
                System.currentTimeMillis(), extra);
        mTransitionLogs.add(tLog);
    }

    private static void logd(String message) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, message);
        }
    }

    @Override
    public DriverDistractionChangeEvent getLastDistractionEvent() throws RemoteException {
        IExperimentalCarImpl.assertPermission(mContext,
                ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION);
        synchronized (mLock) {
            return mCurrentDistractionEvent;
        }
    }

    @Override
    public void addDriverDistractionChangeListener(IDriverDistractionChangeListener listener)
            throws RemoteException {
        IExperimentalCarImpl.assertPermission(mContext,
                ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION);
        if (listener == null) {
            throw new IllegalArgumentException("IDriverDistractionChangeListener is null");
        }
        mDistractionClients.register(listener);

        DriverDistractionChangeEvent changeEvent = mCurrentDistractionEvent;
        mClientDispatchHandler.post(() -> {
            try {
                listener.onDriverDistractionChange(changeEvent);
            } catch (RemoteException ignores) {
                // ignore
            }
        });
    }


    @Override
    public void removeDriverDistractionChangeListener(IDriverDistractionChangeListener listener)
            throws RemoteException {
        IExperimentalCarImpl.assertPermission(mContext,
                ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION);
        if (listener == null) {
            Log.e(TAG, "unregisterUxRestrictionsChangeListener(): listener null");
            throw new IllegalArgumentException("Listener is null");
        }
        mDistractionClients.unregister(listener);
    }

    /**
     * The service connection between this distraction service and a {@link
     * DriverAwarenessSupplierService}, communicated through {@link IDriverAwarenessSupplier}.
     */
    private class DriverAwarenessServiceConnection implements ServiceConnection {

        final int mPriority;

        /**
         * Create an instance of {@link DriverAwarenessServiceConnection}.
         *
         * @param priority the priority of the {@link DriverAwarenessSupplierService} that this
         *                 connection is for
         */
        DriverAwarenessServiceConnection(int priority) {
            mPriority = priority;
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            logd("onServiceConnected, name: " + name + ", binder: " + binder);
            IDriverAwarenessSupplier service = IDriverAwarenessSupplier.Stub.asInterface(
                    binder);
            addDriverAwarenessSupplier(name, service, mPriority);
            try {
                service.setCallback(new DriverAwarenessSupplierCallback(name));
                service.onReady();
            } catch (RemoteException e) {
                Log.e(TAG, "Unable to call onReady on supplier", e);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            logd("onServiceDisconnected, name: " + name);
            removeDriverAwarenessSupplier(name);
            // TODO(b/146471650) rebind to driver awareness suppliers on service disconnect
        }
    }

    /**
     * Driver awareness listener that keeps a references to some attributes of the supplier.
     */
    private class DriverAwarenessSupplierCallback extends IDriverAwarenessSupplierCallback.Stub {

        private final ComponentName mComponentName;

        /**
         * Construct an instance  of {@link DriverAwarenessSupplierCallback}.
         *
         * @param componentName the driver awareness supplier for this listener
         */
        DriverAwarenessSupplierCallback(ComponentName componentName) {
            mComponentName = componentName;
        }

        @Override
        public void onDriverAwarenessUpdated(DriverAwarenessEvent event) {
            IDriverAwarenessSupplier supplier;
            long maxStaleness;
            synchronized (mLock) {
                supplier = mSupplierBinders.get(mComponentName);
                maxStaleness = mSupplierConfigs.get(supplier).getMaxStalenessMillis();
            }
            if (supplier == null) {
                // this should never happen. Initialization process would not be correct.
                throw new IllegalStateException(
                        "No supplier registered for component " + mComponentName);
            }
            logd(String.format("Driver awareness updated for %s: %s",
                    supplier.getClass().getSimpleName(), event));
            handleDriverAwarenessEvent(
                    new DriverAwarenessEventWrapper(event, supplier, maxStaleness));
        }

        @Override
        public void onConfigLoaded(DriverAwarenessSupplierConfig config) throws RemoteException {
            synchronized (mLock) {
                mSupplierConfigs.put(mSupplierBinders.get(mComponentName), config);
            }
        }
    }

    /**
     * Wrapper for {@link DriverAwarenessEvent} that includes some information from the supplier
     * that emitted the event.
     */
    @VisibleForTesting
    static class DriverAwarenessEventWrapper {
        final DriverAwarenessEvent mAwarenessEvent;
        final IDriverAwarenessSupplier mSupplier;
        final long mMaxStaleness;

        /**
         * Construct an instance of {@link DriverAwarenessEventWrapper}.
         *
         * @param awarenessEvent the driver awareness event being wrapped
         * @param supplier       the driver awareness supplier for this listener
         * @param maxStaleness   the max staleness of the supplier that emitted this event (included
         *                       to avoid making a binder call)
         */
        DriverAwarenessEventWrapper(
                DriverAwarenessEvent awarenessEvent,
                IDriverAwarenessSupplier supplier,
                long maxStaleness) {
            mAwarenessEvent = awarenessEvent;
            mSupplier = supplier;
            mMaxStaleness = maxStaleness;
        }

        @Override
        public String toString() {
            return String.format(
                    "DriverAwarenessEventWrapper{mAwarenessChangeEvent=%s, mSupplier=%s, "
                            + "mMaxStaleness=%s}",
                    mAwarenessEvent, mSupplier, mMaxStaleness);
        }
    }
}
