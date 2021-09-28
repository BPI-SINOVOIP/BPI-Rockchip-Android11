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

package android.car.watchdog;

import static com.android.internal.util.function.pooled.PooledLambda.obtainMessage;

import android.annotation.CallbackExecutor;
import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.car.Car;
import android.car.CarManagerBase;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.Preconditions;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;
import java.util.concurrent.Executor;

/**
 * Provides APIs and interfaces for client health checking.
 *
 * @hide
 */
@SystemApi
public final class CarWatchdogManager extends CarManagerBase {

    private static final String TAG = CarWatchdogManager.class.getSimpleName();
    private static final boolean DEBUG = false; // STOPSHIP if true
    private static final int INVALID_SESSION_ID = -1;
    private static final int NUMBER_OF_CONDITIONS_TO_BE_MET = 2;
    // Message ID representing main thread activeness checking.
    private static final int WHAT_CHECK_MAIN_THREAD = 1;

    /** Timeout for services which should be responsive. The length is 3,000 milliseconds. */
    public static final int TIMEOUT_CRITICAL = 0;

    /** Timeout for services which are relatively responsive. The length is 5,000 milliseconds. */
    public static final int TIMEOUT_MODERATE = 1;

    /** Timeout for all other services. The length is 10,000 milliseconds. */
    public static final int TIMEOUT_NORMAL = 2;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "TIMEOUT_", value = {
            TIMEOUT_CRITICAL,
            TIMEOUT_MODERATE,
            TIMEOUT_NORMAL,
    })
    @Target({ElementType.TYPE_USE})
    public @interface TimeoutLengthEnum {}

    private final ICarWatchdogService mService;
    private final ICarWatchdogClientImpl mClientImpl;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private CarWatchdogClientCallback mRegisteredClient;
    @GuardedBy("mLock")
    private Executor mCallbackExecutor;
    @GuardedBy("mLock")
    private SessionInfo mSession = new SessionInfo(INVALID_SESSION_ID, INVALID_SESSION_ID);
    @GuardedBy("mLock")
    private int mRemainingConditions;

    /**
     * CarWatchdogClientCallback is implemented by the clients which want to be health-checked by
     * car watchdog server. Every time onCheckHealthStatus is called, they are expected to
     * respond by calling {@link CarWatchdogManager.tellClientAlive} within timeout. If they don't
     * respond, car watchdog server reports the current state and kills them.
     *
     * <p>Before car watchdog server kills the client, it calls onPrepareProcessTermination to allow
     * them to prepare the termination. They will be killed in 1 second.
     */
    public abstract static class CarWatchdogClientCallback {
        /**
         * Car watchdog server pings the client to check if it is alive.
         *
         * <p>The callback method is called at the Executor which is specifed in {@link
         * #registerClient}.
         *
         * @param sessionId Unique id to distinguish each health checking.
         * @param timeout Time duration within which the client should respond.
         *
         * @return whether the response is immediately acknowledged. If {@code true}, car watchdog
         *         server considers that the response is acknowledged already. If {@code false},
         *         the client should call {@link CarWatchdogManager.tellClientAlive} later to tell
         *         that it is alive.
         */
        public boolean onCheckHealthStatus(int sessionId, @TimeoutLengthEnum int timeout) {
            return false;
        }

        /**
         * Car watchdog server notifies the client that it will be terminated in 1 second.
         *
         * <p>The callback method is called at the Executor which is specifed in {@link
         * #registerClient}.
         */
        public void onPrepareProcessTermination() {}
    }

    /** @hide */
    public CarWatchdogManager(Car car, IBinder service) {
        super(car);
        mService = ICarWatchdogService.Stub.asInterface(service);
        mClientImpl = new ICarWatchdogClientImpl(this);
    }

    /**
     * Registers the car watchdog clients to {@link CarWatchdogManager}.
     *
     * <p>It is allowed to register a client from any thread, but only one client can be
     * registered. If two or more clients are needed, create a new {@link Car} and register a client
     * to it.
     *
     * @param client Watchdog client implementing {@link CarWatchdogClientCallback} interface.
     * @param timeout The time duration within which the client desires to respond. The actual
     *        timeout is decided by watchdog server.
     * @throws IllegalStateException if at least one client is already registered.
     */
    @RequiresPermission(Car.PERMISSION_USE_CAR_WATCHDOG)
    public void registerClient(@NonNull @CallbackExecutor Executor executor,
            @NonNull CarWatchdogClientCallback client, @TimeoutLengthEnum int timeout) {
        synchronized (mLock) {
            if (mRegisteredClient == client) {
                return;
            }
            if (mRegisteredClient != null) {
                throw new IllegalStateException(
                        "Cannot register the client. Only one client can be registered.");
            }
            mRegisteredClient = client;
            mCallbackExecutor = executor;
        }
        try {
            mService.registerClient(mClientImpl, timeout);
            if (DEBUG) {
                Log.d(TAG, "Car watchdog client is successfully registered");
            }
        } catch (RemoteException e) {
            synchronized (mLock) {
                mRegisteredClient = null;
            }
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Unregisters the car watchdog client from {@link CarWatchdogManager}.
     *
     * @param client Watchdog client implementing {@link CarWatchdogClientCallback} interface.
     */
    @RequiresPermission(Car.PERMISSION_USE_CAR_WATCHDOG)
    public void unregisterClient(@NonNull CarWatchdogClientCallback client) {
        synchronized (mLock) {
            if (mRegisteredClient != client) {
                Log.w(TAG, "Cannot unregister the client. It has not been registered.");
                return;
            }
            mRegisteredClient = null;
            mCallbackExecutor = null;
        }
        try {
            mService.unregisterClient(mClientImpl);
            if (DEBUG) {
                Log.d(TAG, "Car watchdog client is successfully unregistered");
            }
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Tells {@link CarWatchdogManager} that the client is alive.
     *
     * @param client Watchdog client implementing {@link CarWatchdogClientCallback} interface.
     * @param sessionId Session id given by {@link CarWatchdogManager}.
     * @throws IllegalStateException if {@code client} is not registered.
     * @throws IllegalArgumentException if {@code session Id} is not correct.
     */
    @RequiresPermission(Car.PERMISSION_USE_CAR_WATCHDOG)
    public void tellClientAlive(@NonNull CarWatchdogClientCallback client, int sessionId) {
        boolean shouldReport;
        synchronized (mLock) {
            if (mRegisteredClient != client) {
                throw new IllegalStateException(
                        "Cannot report client status. The client has not been registered.");
            }
            Preconditions.checkArgument(sessionId != -1 && mSession.currentId == sessionId,
                    "Cannot report client status. "
                    + "The given session id doesn't match the current one.");
            if (mSession.lastReportedId == sessionId) {
                Log.w(TAG, "The given session id is already reported.");
                return;
            }
            mSession.lastReportedId = sessionId;
            mRemainingConditions--;
            shouldReport = checkConditionLocked();
        }
        if (shouldReport) {
            reportToService(sessionId);
        }
    }

    /** @hide */
    @Override
    public void onCarDisconnected() {
        // nothing to do
    }

    private void checkClientStatus(int sessionId, int timeout) {
        CarWatchdogClientCallback client;
        Executor executor;
        mMainHandler.removeMessages(WHAT_CHECK_MAIN_THREAD);
        synchronized (mLock) {
            if (mRegisteredClient == null) {
                Log.w(TAG, "Cannot check client status. The client has not been registered.");
                return;
            }
            mSession.currentId = sessionId;
            client = mRegisteredClient;
            executor = mCallbackExecutor;
            mRemainingConditions = NUMBER_OF_CONDITIONS_TO_BE_MET;
        }
        // For a car watchdog client to be active, 1) its main thread is active and 2) the client
        // responds within timeout. When each condition is met, the remaining task counter is
        // decreased. If the remaining task counter is zero, the client is considered active.
        mMainHandler.sendMessage(obtainMessage(CarWatchdogManager::checkMainThread, this)
                .setWhat(WHAT_CHECK_MAIN_THREAD));
        // Call the client callback to check if the client is active.
        executor.execute(() -> {
            boolean checkDone = client.onCheckHealthStatus(sessionId, timeout);
            if (checkDone) {
                boolean shouldReport;
                synchronized (mLock) {
                    if (mSession.lastReportedId == sessionId) {
                        return;
                    }
                    mSession.lastReportedId = sessionId;
                    mRemainingConditions--;
                    shouldReport = checkConditionLocked();
                }
                if (shouldReport) {
                    reportToService(sessionId);
                }
            }
        });
    }

    private void checkMainThread() {
        int sessionId;
        boolean shouldReport;
        synchronized (mLock) {
            mRemainingConditions--;
            sessionId = mSession.currentId;
            shouldReport = checkConditionLocked();
        }
        if (shouldReport) {
            reportToService(sessionId);
        }
    }

    private boolean checkConditionLocked() {
        if (mRemainingConditions < 0) {
            Log.wtf(TAG, "Remaining condition is less than zero: should not happen");
        }
        return mRemainingConditions == 0;
    }

    private void reportToService(int sessionId) {
        try {
            mService.tellClientAlive(mClientImpl, sessionId);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    private void notifyProcessTermination() {
        CarWatchdogClientCallback client;
        Executor executor;
        synchronized (mLock) {
            if (mRegisteredClient == null) {
                Log.w(TAG, "Cannot notify the client. The client has not been registered.");
                return;
            }
            client = mRegisteredClient;
            executor = mCallbackExecutor;
        }
        executor.execute(() -> client.onPrepareProcessTermination());
    }

    /** @hide */
    private static final class ICarWatchdogClientImpl extends ICarWatchdogServiceCallback.Stub {
        private final WeakReference<CarWatchdogManager> mManager;

        private ICarWatchdogClientImpl(CarWatchdogManager manager) {
            mManager = new WeakReference<>(manager);
        }

        @Override
        public void onCheckHealthStatus(int sessionId, int timeout) {
            CarWatchdogManager manager = mManager.get();
            if (manager != null) {
                manager.checkClientStatus(sessionId, timeout);
            }
        }

        @Override
        public void onPrepareProcessTermination() {
            CarWatchdogManager manager = mManager.get();
            if (manager != null) {
                manager.notifyProcessTermination();
            }
        }
    }

    private final class SessionInfo {
        public int currentId;
        public int lastReportedId;

        SessionInfo(int currentId, int lastReportedId) {
            this.currentId = currentId;
            this.lastReportedId = lastReportedId;
        }
    }
}
