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

package android.car.watchdoglib;

import static com.android.internal.util.function.pooled.PooledLambda.obtainMessage;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.automotive.watchdog.ICarWatchdog;
import android.automotive.watchdog.ICarWatchdogClient;
import android.automotive.watchdog.ICarWatchdogMonitor;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import java.util.Objects;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Helper class for car watchdog daemon.
 *
 * @hide
 */
public final class CarWatchdogDaemonHelper {

    private static final String TAG = CarWatchdogDaemonHelper.class.getSimpleName();
    // Carwatchdog daemon polls for the service manager status once every 250 milliseconds.
    // CAR_WATCHDOG_DAEMON_BIND_RETRY_INTERVAL_MS value should be at least twice the poll interval
    // used by the daemon.
    private static final long CAR_WATCHDOG_DAEMON_BIND_RETRY_INTERVAL_MS = 500;
    private static final long CAR_WATCHDOG_DAEMON_FIND_MARGINAL_TIME_MS = 300;
    private static final int CAR_WATCHDOG_DAEMON_BIND_MAX_RETRY = 3;
    private static final String CAR_WATCHDOG_DAEMON_INTERFACE =
            "android.automotive.watchdog.ICarWatchdog/default";

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final CopyOnWriteArrayList<OnConnectionChangeListener> mConnectionListeners =
            new CopyOnWriteArrayList<>();
    private final String mTag;
    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private @Nullable ICarWatchdog mCarWatchdogDaemon;
    @GuardedBy("mLock")
    private boolean mConnectionInProgress;

    private final IBinder.DeathRecipient mDeathRecipient = new IBinder.DeathRecipient() {
        @Override
        public void binderDied() {
            Log.w(mTag, "Car watchdog daemon died: reconnecting");
            unlinkToDeath();
            synchronized (mLock) {
                mCarWatchdogDaemon = null;
            }
            for (OnConnectionChangeListener listener : mConnectionListeners) {
                listener.onConnectionChange(false);
            }
            mHandler.sendMessageDelayed(obtainMessage(CarWatchdogDaemonHelper::connectToDaemon,
                    CarWatchdogDaemonHelper.this, CAR_WATCHDOG_DAEMON_BIND_MAX_RETRY),
                    CAR_WATCHDOG_DAEMON_BIND_RETRY_INTERVAL_MS);
        }
    };

    private interface Invokable {
        void invoke(ICarWatchdog daemon) throws RemoteException;
    }

    /**
     * Listener to notify the state change of the connection to car watchdog daemon.
     */
    public interface OnConnectionChangeListener {
        /** Gets called when car watchdog daemon is connected or disconnected. */
        void onConnectionChange(boolean connected);
    }

    public CarWatchdogDaemonHelper() {
        mTag = TAG;
    }

    public CarWatchdogDaemonHelper(@NonNull String requestor) {
        mTag = TAG + "[" + requestor + "]";
    }

    /**
     * Connects to car watchdog daemon.
     *
     * <p>When it's connected, {@link OnConnectionChangeListener} is called with
     * {@code true}.
     */
    public void connect() {
        synchronized (mLock) {
            if (mCarWatchdogDaemon != null || mConnectionInProgress) {
                return;
            }
            mConnectionInProgress = true;
        }
        connectToDaemon(CAR_WATCHDOG_DAEMON_BIND_MAX_RETRY);
    }

    /**
     * Disconnects from car watchdog daemon.
     *
     * <p>When it's disconnected, {@link OnConnectionChangeListener} is called with
     * {@code false}.
     */
    public void disconnect() {
        unlinkToDeath();
        synchronized (mLock) {
            mCarWatchdogDaemon = null;
        }
    }

    /**
     * Adds {@link OnConnectionChangeListener}.
     *
     * @param listener Listener to be notified when connection state changes.
     */
    public void addOnConnectionChangeListener(
            @NonNull OnConnectionChangeListener listener) {
        Objects.requireNonNull(listener, "Listener cannot be null");
        mConnectionListeners.add(listener);
    }

    /**
     * Removes {@link OnConnectionChangeListener}.
     *
     * @param listener Listener to be removed.
     */
    public void removeOnConnectionChangeListener(
            @NonNull OnConnectionChangeListener listener) {
        Objects.requireNonNull(listener, "Listener cannot be null");
        mConnectionListeners.remove(listener);
    }

    /**
     * Registers car watchdog client.
     *
     * @param client Car watchdog client to be registered.
     * @param timeout Time within which the client should respond.
     * @throws IllegalArgumentException If the client is already registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void registerClient(ICarWatchdogClient client, int timeout) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.registerClient(client, timeout));
    }

    /**
     * Unregisters car watchdog client.
     *
     * @param client Car watchdog client to be unregistered.
     * @throws IllegalArgumentException If the client is not registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void unregisterClient(ICarWatchdogClient client) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.unregisterClient(client));
    }

    /**
     * Registers car watchdog client as mediator.
     *
     * @param mediator Car watchdog client to be registered.
     * @throws IllegalArgumentException If the mediator is already registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void registerMediator(ICarWatchdogClient mediator) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.registerMediator(mediator));
    }

    /**
     * Unregisters car watchdog client as mediator.
     *
     * @param mediator Car watchdog client to be unregistered.
     * @throws IllegalArgumentException If the mediator is not registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void unregisterMediator(ICarWatchdogClient mediator)  throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.unregisterMediator(mediator));
    }

    /**
     * Registers car watchdog monitor.
     *
     * @param monitor Car watchdog monitor to be registered.
     * @throws IllegalArgumentException If there is another monitor registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void registerMonitor(ICarWatchdogMonitor monitor)  throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.registerMonitor(monitor));
    }

    /**
     * Unregisters car watchdog monitor.
     *
     * @param monitor Car watchdog monitor to be unregistered.
     * @throws IllegalArgumentException If the monitor is not registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void unregisterMonitor(ICarWatchdogMonitor monitor) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.unregisterMonitor(monitor));
    }

    /**
     * Tells car watchdog daemon that the client is alive.
     *
     * @param client Car watchdog client which has been pined by car watchdog daemon.
     * @param sessionId Session ID that car watchdog daemon has given.
     * @throws IllegalArgumentException If the client is not registered,
     *                                  or session ID is not correct.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void tellClientAlive(ICarWatchdogClient client, int sessionId) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.tellClientAlive(client, sessionId));
    }

    /**
     * Tells car watchdog daemon that the mediator is alive.
     *
     * @param mediator Car watchdog client which has been pined by car watchdog daemon.
     * @param clientsNotResponding Array of process ID that are not responding.
     * @param sessionId Session ID that car watchdog daemon has given.
     * @throws IllegalArgumentException If the client is not registered,
     *                                  or session ID is not correct.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void tellMediatorAlive(ICarWatchdogClient mediator, int[] clientsNotResponding,
            int sessionId) throws RemoteException {
        invokeDaemonMethod(
                (daemon) -> daemon.tellMediatorAlive(mediator, clientsNotResponding, sessionId));
    }

    /**
     * Tells car watchdog daemon that the monitor has dumped clients' process information.
     *
     * @param monitor Car watchdog monitor that dumped process information.
     * @param pid ID of process that has been dumped.
     * @throws IllegalArgumentException If the monitor is not registered.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void tellDumpFinished(ICarWatchdogMonitor monitor, int pid) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.tellDumpFinished(monitor, pid));
    }

    /**
     * Tells car watchdog daemon that system state has been changed for the specified StateType.
     *
     * @param type Either PowerCycle, UserState, or BootPhase
     * @param arg1 First state change information for the specified state type.
     * @param arg2 Second state change information for the specified state type.
     * @throws IllegalArgumentException If the args don't match the state type. Refer to the aidl
     *                                  interface for more information on the args.
     * @throws IllegalStateException If car watchdog daemon is not connected.
     * @throws RemoteException
     */
    public void notifySystemStateChange(int type, int arg1, int arg2) throws RemoteException {
        invokeDaemonMethod((daemon) -> daemon.notifySystemStateChange(type, arg1, arg2));
    }

    private void invokeDaemonMethod(Invokable r) throws RemoteException {
        ICarWatchdog daemon;
        synchronized (mLock) {
            if (mCarWatchdogDaemon == null) {
                throw new IllegalStateException("Car watchdog daemon is not connected");
            }
            daemon = mCarWatchdogDaemon;
        }
        r.invoke(daemon);
    }

    private void connectToDaemon(int retryCount) {
        if (retryCount <= 0) {
            synchronized (mLock) {
                mConnectionInProgress = false;
            }
            Log.e(mTag, "Cannot reconnect to car watchdog daemon after retrying "
                    + CAR_WATCHDOG_DAEMON_BIND_MAX_RETRY + " times");
            return;
        }
        if (makeBinderConnection()) {
            Log.i(mTag, "Connected to car watchdog daemon");
            return;
        }
        mHandler.sendMessageDelayed(obtainMessage(CarWatchdogDaemonHelper::connectToDaemon,
                CarWatchdogDaemonHelper.this, retryCount - 1),
                CAR_WATCHDOG_DAEMON_BIND_RETRY_INTERVAL_MS);
    }

    private boolean makeBinderConnection() {
        long currentTimeMs = SystemClock.uptimeMillis();
        IBinder binder = ServiceManager.getService(CAR_WATCHDOG_DAEMON_INTERFACE);
        if (binder == null) {
            Log.w(mTag, "Getting car watchdog daemon binder failed");
            return false;
        }
        long elapsedTimeMs = SystemClock.uptimeMillis() - currentTimeMs;
        if (elapsedTimeMs > CAR_WATCHDOG_DAEMON_FIND_MARGINAL_TIME_MS) {
            Log.wtf(mTag, "Finding car watchdog daemon took too long(" + elapsedTimeMs + "ms)");
        }

        ICarWatchdog daemon = ICarWatchdog.Stub.asInterface(binder);
        if (daemon == null) {
            Log.w(mTag, "Getting car watchdog daemon interface failed");
            return false;
        }
        synchronized (mLock) {
            mCarWatchdogDaemon = daemon;
            mConnectionInProgress = false;
        }
        linkToDeath();
        for (OnConnectionChangeListener listener : mConnectionListeners) {
            listener.onConnectionChange(true);
        }
        return true;
    }

    private void linkToDeath() {
        IBinder binder;
        synchronized (mLock) {
            if (mCarWatchdogDaemon == null) {
                return;
            }
            binder = mCarWatchdogDaemon.asBinder();
        }
        if (binder == null) {
            Log.w(mTag, "Linking to binder death recipient skipped");
            return;
        }
        try {
            binder.linkToDeath(mDeathRecipient, 0);
        } catch (RemoteException e) {
            Log.w(mTag, "Linking to binder death recipient failed: " + e);
        }
    }

    private void unlinkToDeath() {
        IBinder binder;
        synchronized (mLock) {
            if (mCarWatchdogDaemon == null) {
                return;
            }
            binder = mCarWatchdogDaemon.asBinder();
        }
        if (binder == null) {
            Log.w(mTag, "Unlinking from binder death recipient skipped");
            return;
        }
        binder.unlinkToDeath(mDeathRecipient, 0);
    }
}
