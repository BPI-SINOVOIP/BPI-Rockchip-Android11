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

package com.android.car.watchdog;

import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STARTING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPED;
import static android.car.watchdog.CarWatchdogManager.TIMEOUT_CRITICAL;
import static android.car.watchdog.CarWatchdogManager.TIMEOUT_MODERATE;
import static android.car.watchdog.CarWatchdogManager.TIMEOUT_NORMAL;

import static com.android.car.CarLog.TAG_WATCHDOG;
import static com.android.internal.util.function.pooled.PooledLambda.obtainMessage;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.automotive.watchdog.ICarWatchdogClient;
import android.automotive.watchdog.PowerCycle;
import android.automotive.watchdog.StateType;
import android.automotive.watchdog.UserState;
import android.car.hardware.power.CarPowerManager.CarPowerStateListener;
import android.car.hardware.power.ICarPowerStateListener;
import android.car.watchdog.ICarWatchdogService;
import android.car.watchdog.ICarWatchdogServiceCallback;
import android.car.watchdoglib.CarWatchdogDaemonHelper;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import com.android.car.CarLocalServices;
import com.android.car.CarPowerManagementService;
import com.android.car.CarServiceBase;
import com.android.car.user.CarUserService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Service to implement CarWatchdogManager API.
 *
 * <p>CarWatchdogService runs as car watchdog mediator, which checks clients' health status and
 * reports the result to car watchdog server.
 */
public final class CarWatchdogService extends ICarWatchdogService.Stub implements CarServiceBase {

    private static final boolean DEBUG = false; // STOPSHIP if true
    private static final String TAG = TAG_WATCHDOG;
    private static final int[] ALL_TIMEOUTS =
            { TIMEOUT_CRITICAL, TIMEOUT_MODERATE, TIMEOUT_NORMAL };

    private final Context mContext;
    private final ICarWatchdogClientImpl mWatchdogClient;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private final CarWatchdogDaemonHelper mCarWatchdogDaemonHelper;
    private final CarWatchdogDaemonHelper.OnConnectionChangeListener mConnectionListener =
            (connected) -> {
                if (connected) {
                    registerToDaemon();
                }
            };
    private final Object mLock = new Object();
    /*
     * Keeps the list of car watchdog client according to timeout:
     * key => timeout, value => ClientInfo list.
     * The value of SparseArray is guarded by mLock.
     */
    @GuardedBy("mLock")
    private final SparseArray<ArrayList<ClientInfo>> mClientMap = new SparseArray<>();
    /*
     * Keeps the map of car watchdog client being checked by CarWatchdogService according to
     * timeout: key => timeout, value => ClientInfo map.
     * The value is also a map: key => session id, value => ClientInfo.
     */
    @GuardedBy("mLock")
    private final SparseArray<SparseArray<ClientInfo>> mPingedClientMap = new SparseArray<>();
    /*
     * Keeps whether client health checking is being performed according to timeout:
     * key => timeout, value => boolean (whether client health checking is being performed).
     * The value of SparseArray is guarded by mLock.
     */
    @GuardedBy("mLock")
    private final SparseArray<Boolean> mClientCheckInProgress = new SparseArray<>();
    @GuardedBy("mLock")
    private final ArrayList<ClientInfo> mClientsNotResponding = new ArrayList<>();
    @GuardedBy("mMainHandler")
    private int mLastSessionId;
    @GuardedBy("mMainHandler")
    private final SparseBooleanArray mStoppedUser = new SparseBooleanArray();

    @VisibleForTesting
    public CarWatchdogService(Context context) {
        mContext = context;
        mCarWatchdogDaemonHelper = new CarWatchdogDaemonHelper(TAG_WATCHDOG);
        mWatchdogClient = new ICarWatchdogClientImpl(this);
    }

    @Override
    public void init() {
        for (int timeout : ALL_TIMEOUTS) {
            mClientMap.put(timeout, new ArrayList<ClientInfo>());
            mPingedClientMap.put(timeout, new SparseArray<ClientInfo>());
            mClientCheckInProgress.put(timeout, false);
        }
        subscribePowerCycleChange();
        subscribeUserStateChange();
        mCarWatchdogDaemonHelper.addOnConnectionChangeListener(mConnectionListener);
        mCarWatchdogDaemonHelper.connect();
        if (DEBUG) {
            Log.d(TAG, "CarWatchdogService is initialized");
        }
    }

    @Override
    public void release() {
        unregisterFromDaemon();
        mCarWatchdogDaemonHelper.disconnect();
    }

    @Override
    public void dump(PrintWriter writer) {
        String indent = "  ";
        int count = 1;
        synchronized (mLock) {
            writer.println("*CarWatchdogService*");
            writer.println("Registered clients");
            for (int timeout : ALL_TIMEOUTS) {
                ArrayList<ClientInfo> clients = mClientMap.get(timeout);
                String timeoutStr = timeoutToString(timeout);
                for (int i = 0; i < clients.size(); i++, count++) {
                    ClientInfo clientInfo = clients.get(i);
                    writer.printf("%sclient #%d: timeout = %s, pid = %d\n",
                            indent, count, timeoutStr, clientInfo.pid);
                }
            }
            writer.printf("Stopped users: ");
            int size = mStoppedUser.size();
            if (size > 0) {
                writer.printf("%d", mStoppedUser.keyAt(0));
                for (int i = 1; i < size; i++) {
                    writer.printf(", %d", mStoppedUser.keyAt(i));
                }
                writer.printf("\n");
            } else {
                writer.printf("none\n");
            }
        }
    }

    /**
     * Registers {@link android.car.watchdog.ICarWatchdogServiceCallback} to
     * {@link CarWatchdogService}.
     */
    @Override
    public void registerClient(ICarWatchdogServiceCallback client, int timeout) {
        ArrayList<ClientInfo> clients = mClientMap.get(timeout);
        if (clients == null) {
            Log.w(TAG, "Cannot register the client: invalid timeout");
            return;
        }
        synchronized (mLock) {
            IBinder binder = client.asBinder();
            for (int i = 0; i < clients.size(); i++) {
                ClientInfo clientInfo = clients.get(i);
                if (binder == clientInfo.client.asBinder()) {
                    Log.w(TAG,
                            "Cannot register the client: the client(pid:" + clientInfo.pid
                            + ") has been already registered");
                    return;
                }
            }
            int pid = Binder.getCallingPid();
            int userId = UserHandle.getUserId(Binder.getCallingUid());
            ClientInfo clientInfo = new ClientInfo(client, pid, userId, timeout);
            try {
                clientInfo.linkToDeath();
            } catch (RemoteException e) {
                Log.w(TAG, "Cannot register the client: linkToDeath to the client failed");
                return;
            }
            clients.add(clientInfo);
            if (DEBUG) {
                Log.d(TAG, "Client(pid: " + pid + ") is registered");
            }
        }
    }

    /**
     * Unregisters {@link android.car.watchdog.ICarWatchdogServiceCallback} from
     * {@link CarWatchdogService}.
     */
    @Override
    public void unregisterClient(ICarWatchdogServiceCallback client) {
        synchronized (mLock) {
            IBinder binder = client.asBinder();
            for (int timeout : ALL_TIMEOUTS) {
                ArrayList<ClientInfo> clients = mClientMap.get(timeout);
                for (int i = 0; i < clients.size(); i++) {
                    ClientInfo clientInfo = clients.get(i);
                    if (binder != clientInfo.client.asBinder()) {
                        continue;
                    }
                    clientInfo.unlinkToDeath();
                    clients.remove(i);
                    if (DEBUG) {
                        Log.d(TAG, "Client(pid: " + clientInfo.pid + ") is unregistered");
                    }
                    return;
                }
            }
        }
        Log.w(TAG, "Cannot unregister the client: the client has not been registered before");
        return;
    }

    /**
     * Tells {@link CarWatchdogService} that the client is alive.
     */
    @Override
    public void tellClientAlive(ICarWatchdogServiceCallback client, int sessionId) {
        synchronized (mLock) {
            for (int timeout : ALL_TIMEOUTS) {
                if (!mClientCheckInProgress.get(timeout)) {
                    continue;
                }
                SparseArray<ClientInfo> pingedClients = mPingedClientMap.get(timeout);
                ClientInfo clientInfo = pingedClients.get(sessionId);
                if (clientInfo != null && clientInfo.client.asBinder() == client.asBinder()) {
                    pingedClients.remove(sessionId);
                    return;
                }
            }
        }
    }

    @VisibleForTesting
    protected int getClientCount(int timeout) {
        synchronized (mLock) {
            ArrayList<ClientInfo> clients = mClientMap.get(timeout);
            return clients != null ? clients.size() : 0;
        }
    }

    private void registerToDaemon() {
        try {
            mCarWatchdogDaemonHelper.registerMediator(mWatchdogClient);
            if (DEBUG) {
                Log.d(TAG, "CarWatchdogService registers to car watchdog daemon");
            }
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "Cannot register to car watchdog daemon: " + e);
        }
        UserManager userManager = UserManager.get(mContext);
        List<UserInfo> users = userManager.getUsers();
        try {
            // TODO(b/152780162): reduce the number of RPC calls(isUserRunning).
            for (UserInfo info : users) {
                int userState = userManager.isUserRunning(info.id)
                        ? UserState.USER_STATE_STARTED : UserState.USER_STATE_STOPPED;
                mCarWatchdogDaemonHelper.notifySystemStateChange(StateType.USER_STATE, info.id,
                        userState);
                if (userState == UserState.USER_STATE_STOPPED) {
                    mStoppedUser.put(info.id, true);
                } else {
                    mStoppedUser.delete(info.id);
                }
            }
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "Notifying system state change failed: " + e);
        }
    }

    private void unregisterFromDaemon() {
        try {
            mCarWatchdogDaemonHelper.unregisterMediator(mWatchdogClient);
            if (DEBUG) {
                Log.d(TAG, "CarWatchdogService unregisters from car watchdog daemon");
            }
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "Cannot unregister from car watchdog daemon: " + e);
        }
    }

    private void onClientDeath(ICarWatchdogServiceCallback client, int timeout) {
        synchronized (mLock) {
            removeClientLocked(client.asBinder(), timeout);
        }
    }

    private void postHealthCheckMessage(int sessionId) {
        mMainHandler.sendMessage(obtainMessage(CarWatchdogService::doHealthCheck, this, sessionId));
    }

    private void doHealthCheck(int sessionId) {
        // For critical clients, the response status are checked just before reporting to car
        // watchdog daemon. For moderate and normal clients, the status are checked after allowed
        // delay per timeout.
        analyzeClientResponse(TIMEOUT_CRITICAL);
        reportHealthCheckResult(sessionId);
        sendPingToClients(TIMEOUT_CRITICAL);
        sendPingToClientsAndCheck(TIMEOUT_MODERATE);
        sendPingToClientsAndCheck(TIMEOUT_NORMAL);
    }

    private void analyzeClientResponse(int timeout) {
        // Clients which are not responding are stored in mClientsNotResponding, and will be dumped
        // and killed at the next response of CarWatchdogService to car watchdog daemon.
        SparseArray<ClientInfo> pingedClients = mPingedClientMap.get(timeout);
        synchronized (mLock) {
            for (int i = 0; i < pingedClients.size(); i++) {
                ClientInfo clientInfo = pingedClients.valueAt(i);
                if (mStoppedUser.get(clientInfo.userId)) {
                    continue;
                }
                mClientsNotResponding.add(clientInfo);
                removeClientLocked(clientInfo.client.asBinder(), timeout);
            }
            mClientCheckInProgress.setValueAt(timeout, false);
        }
    }

    private void sendPingToClients(int timeout) {
        SparseArray<ClientInfo> pingedClients = mPingedClientMap.get(timeout);
        ArrayList<ClientInfo> clientsToCheck;
        synchronized (mLock) {
            pingedClients.clear();
            clientsToCheck = new ArrayList<>(mClientMap.get(timeout));
            for (int i = 0; i < clientsToCheck.size(); i++) {
                ClientInfo clientInfo = clientsToCheck.get(i);
                if (mStoppedUser.get(clientInfo.userId)) {
                    continue;
                }
                int sessionId = getNewSessionId();
                clientInfo.sessionId = sessionId;
                pingedClients.put(sessionId, clientInfo);
            }
            mClientCheckInProgress.setValueAt(timeout, true);
        }
        for (int i = 0; i < clientsToCheck.size(); i++) {
            ClientInfo clientInfo = clientsToCheck.get(i);
            try {
                clientInfo.client.onCheckHealthStatus(clientInfo.sessionId, timeout);
            } catch (RemoteException e) {
                Log.w(TAG, "Sending a ping message to client(pid: " +  clientInfo.pid
                        + ") failed: " + e);
                synchronized (mLock) {
                    pingedClients.remove(clientInfo.sessionId);
                }
            }
        }
    }

    private void sendPingToClientsAndCheck(int timeout) {
        synchronized (mLock) {
            if (mClientCheckInProgress.get(timeout)) {
                return;
            }
        }
        sendPingToClients(timeout);
        mMainHandler.sendMessageDelayed(obtainMessage(CarWatchdogService::analyzeClientResponse,
                this, timeout), timeoutToDurationMs(timeout));
    }

    private int getNewSessionId() {
        if (++mLastSessionId <= 0) {
            mLastSessionId = 1;
        }
        return mLastSessionId;
    }

    private void removeClientLocked(IBinder clientBinder, int timeout) {
        ArrayList<ClientInfo> clients = mClientMap.get(timeout);
        for (int i = 0; i < clients.size(); i++) {
            ClientInfo clientInfo = clients.get(i);
            if (clientBinder == clientInfo.client.asBinder()) {
                clients.remove(i);
                return;
            }
        }
    }

    private void reportHealthCheckResult(int sessionId) {
        int[] clientsNotResponding;
        ArrayList<ClientInfo> clientsToNotify;
        synchronized (mLock) {
            clientsNotResponding = toIntArray(mClientsNotResponding);
            clientsToNotify = new ArrayList<>(mClientsNotResponding);
            mClientsNotResponding.clear();
        }
        for (int i = 0; i < clientsToNotify.size(); i++) {
            ClientInfo clientInfo = clientsToNotify.get(i);
            try {
                clientInfo.client.onPrepareProcessTermination();
            } catch (RemoteException e) {
                Log.w(TAG, "Notifying onPrepareProcessTermination to client(pid: " + clientInfo.pid
                        + ") failed: " + e);
            }
        }

        try {
            mCarWatchdogDaemonHelper.tellMediatorAlive(mWatchdogClient, clientsNotResponding,
                    sessionId);
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "Cannot respond to car watchdog daemon (sessionId=" + sessionId + "): " + e);
        }
    }

    private void subscribePowerCycleChange() {
        CarPowerManagementService powerService =
                CarLocalServices.getService(CarPowerManagementService.class);
        if (powerService == null) {
            Log.w(TAG, "Cannot get CarPowerManagementService");
            return;
        }
        powerService.registerListener(new ICarPowerStateListener.Stub() {
            @Override
            public void onStateChanged(int state) {
                int powerCycle;
                switch (state) {
                    // SHUTDOWN_PREPARE covers suspend and shutdown.
                    case CarPowerStateListener.SHUTDOWN_PREPARE:
                        powerCycle = PowerCycle.POWER_CYCLE_SUSPEND;
                        break;
                    // ON covers resume.
                    case CarPowerStateListener.ON:
                        powerCycle = PowerCycle.POWER_CYCLE_RESUME;
                        // There might be outdated & incorrect info. We should reset them before
                        // starting to do health check.
                        prepareHealthCheck();
                        break;
                    default:
                        return;
                }
                try {
                    mCarWatchdogDaemonHelper.notifySystemStateChange(StateType.POWER_CYCLE,
                            powerCycle, /* arg2= */ -1);
                    if (DEBUG) {
                        Log.d(TAG, "Notified car watchdog daemon a power cycle("
                                + powerCycle + ")");
                    }
                } catch (RemoteException | RuntimeException e) {
                    Log.w(TAG, "Notifying system state change failed: " + e);
                }
            }
        });
    }

    private void subscribeUserStateChange() {
        CarUserService userService = CarLocalServices.getService(CarUserService.class);
        if (userService == null) {
            Log.w(TAG, "Cannot get CarUserService");
            return;
        }
        userService.addUserLifecycleListener((event) -> {
            int userId = event.getUserHandle().getIdentifier();
            int userState;
            String userStateDesc;
            synchronized (mLock) {
                switch (event.getEventType()) {
                    case USER_LIFECYCLE_EVENT_TYPE_STARTING:
                        mStoppedUser.delete(userId);
                        userState = UserState.USER_STATE_STARTED;
                        userStateDesc = "STARTING";
                        break;
                    case USER_LIFECYCLE_EVENT_TYPE_STOPPED:
                        mStoppedUser.put(userId, true);
                        userState = UserState.USER_STATE_STOPPED;
                        userStateDesc = "STOPPING";
                        break;
                    default:
                        return;
                }
            }
            try {
                mCarWatchdogDaemonHelper.notifySystemStateChange(StateType.USER_STATE, userId,
                        userState);
                if (DEBUG) {
                    Log.d(TAG, "Notified car watchdog daemon a user state: userId = " + userId
                            + ", userState = " + userStateDesc);
                }
            } catch (RemoteException | RuntimeException e) {
                Log.w(TAG, "Notifying system state change failed: " + e);
            }
        });
    }

    private void prepareHealthCheck() {
        synchronized (mLock) {
            for (int timeout : ALL_TIMEOUTS) {
                SparseArray<ClientInfo> pingedClients = mPingedClientMap.get(timeout);
                pingedClients.clear();
            }
        }
    }

    @NonNull
    private int[] toIntArray(@NonNull ArrayList<ClientInfo> list) {
        int size = list.size();
        int[] intArray = new int[size];
        for (int i = 0; i < size; i++) {
            intArray[i] = list.get(i).pid;
        }
        return intArray;
    }

    private String timeoutToString(int timeout) {
        switch (timeout) {
            case TIMEOUT_CRITICAL:
                return "critical";
            case TIMEOUT_MODERATE:
                return "moderate";
            case TIMEOUT_NORMAL:
                return "normal";
            default:
                Log.w(TAG, "Unknown timeout value");
                return "unknown";
        }
    }

    private long timeoutToDurationMs(int timeout) {
        switch (timeout) {
            case TIMEOUT_CRITICAL:
                return 3000L;
            case TIMEOUT_MODERATE:
                return 5000L;
            case TIMEOUT_NORMAL:
                return 10000L;
            default:
                Log.w(TAG, "Unknown timeout value");
                return 10000L;
        }
    }

    private static final class ICarWatchdogClientImpl extends ICarWatchdogClient.Stub {
        private final WeakReference<CarWatchdogService> mService;

        private ICarWatchdogClientImpl(CarWatchdogService service) {
            mService = new WeakReference<>(service);
        }

        @Override
        public void checkIfAlive(int sessionId, int timeout) {
            CarWatchdogService service = mService.get();
            if (service == null) {
                Log.w(TAG, "CarWatchdogService is not available");
                return;
            }
            service.postHealthCheckMessage(sessionId);
        }

        @Override
        public void prepareProcessTermination() {
            Log.w(TAG, "CarWatchdogService is about to be killed by car watchdog daemon");
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }

    private final class ClientInfo implements IBinder.DeathRecipient {
        public final ICarWatchdogServiceCallback client;
        public final int pid;
        @UserIdInt public final int userId;
        public final int timeout;
        public volatile int sessionId;

        private ClientInfo(ICarWatchdogServiceCallback client, int pid, @UserIdInt int userId,
                int timeout) {
            this.client = client;
            this.pid = pid;
            this.userId = userId;
            this.timeout = timeout;
        }

        @Override
        public void binderDied() {
            Log.w(TAG, "Client(pid: " + pid + ") died");
            onClientDeath(client, timeout);
        }

        private void linkToDeath() throws RemoteException {
            client.asBinder().linkToDeath(this, 0);
        }

        private void unlinkToDeath() {
            client.asBinder().unlinkToDeath(this, 0);
        }
    }
}
