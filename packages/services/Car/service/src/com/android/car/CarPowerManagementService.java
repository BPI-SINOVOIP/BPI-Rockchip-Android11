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
package com.android.car;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.car.Car;
import android.car.hardware.power.CarPowerManager.CarPowerStateListener;
import android.car.hardware.power.ICarPower;
import android.car.hardware.power.ICarPowerStateListener;
import android.car.userlib.HalCallback;
import android.car.userlib.InitialUserSetter;
import android.car.userlib.InitialUserSetter.InitialUserInfoType;
import android.car.userlib.UserHalHelper;
import android.car.userlib.UserHelper;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoRequestType;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponseAction;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateReq;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateShutdownParam;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.UserManager;
import android.sysprop.CarProperties;
import android.util.AtomicFile;
import android.util.Slog;

import com.android.car.am.ContinuousBlankActivity;
import com.android.car.hal.PowerHalService;
import com.android.car.hal.PowerHalService.PowerState;
import com.android.car.systeminterface.SystemInterface;
import com.android.car.user.CarUserNoticeService;
import com.android.car.user.CarUserService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.app.IVoiceInteractionManagerService;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.lang.ref.WeakReference;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Power Management service class for cars. Controls the power states and interacts with other
 * parts of the system to ensure its own state.
 */
public class CarPowerManagementService extends ICarPower.Stub implements
        CarServiceBase, PowerHalService.PowerEventListener {

    // TODO: replace all usage
    private static final String TAG = CarLog.TAG_POWER;
    private static final String WIFI_STATE_FILENAME = "wifi_state";
    private static final String WIFI_STATE_MODIFIED = "forcibly_disabled";
    private static final String WIFI_STATE_ORIGINAL = "original";
    // If Suspend to RAM fails, we retry with an exponential back-off:
    // The wait interval will be 10 msec, 20 msec, 40 msec, ...
    // Once the wait interval goes beyond 100 msec, it is fixed at 100 msec.
    private static final long INITIAL_SUSPEND_RETRY_INTERVAL_MS = 10;
    private static final long MAX_RETRY_INTERVAL_MS = 100;
    // Minimum and maximum wait duration before the system goes into Suspend to RAM.
    private static final long MIN_SUSPEND_WAIT_DURATION_MS = 0;
    private static final long MAX_SUSPEND_WAIT_DURATION_MS = 3 * 60 * 1000;

    private final Object mLock = new Object();
    private final Object mSimulationWaitObject = new Object();

    private final Context mContext;
    private final PowerHalService mHal;
    private final SystemInterface mSystemInterface;
    // The listeners that complete simply by returning from onStateChanged()
    private final PowerManagerCallbackList mPowerManagerListeners = new PowerManagerCallbackList();
    // The listeners that must indicate asynchronous completion by calling finished().
    private final PowerManagerCallbackList mPowerManagerListenersWithCompletion =
                          new PowerManagerCallbackList();

    @GuardedBy("mSimulationWaitObject")
    private boolean mWakeFromSimulatedSleep;
    @GuardedBy("mSimulationWaitObject")
    private boolean mInSimulatedDeepSleepMode;

    @GuardedBy("mLock")
    private final Set<IBinder> mListenersWeAreWaitingFor = new HashSet<>();
    @GuardedBy("mLock")
    private CpmsState mCurrentState;
    @GuardedBy("mLock")
    private Timer mTimer;
    @GuardedBy("mLock")
    private long mProcessingStartTime;
    @GuardedBy("mLock")
    private long mLastSleepEntryTime;
    @GuardedBy("mLock")
    private final LinkedList<CpmsState> mPendingPowerStates = new LinkedList<>();
    private final HandlerThread mHandlerThread = CarServiceUtils.getHandlerThread(
            getClass().getSimpleName());
    private final PowerHandler mHandler = new PowerHandler(mHandlerThread.getLooper(), this);

    @GuardedBy("mLock")
    private boolean mTimerActive;
    @GuardedBy("mLock")
    private int mNextWakeupSec;
    @GuardedBy("mLock")
    private boolean mShutdownOnFinish;
    @GuardedBy("mLock")
    private boolean mShutdownOnNextSuspend;
    @GuardedBy("mLock")
    private boolean mIsBooting = true;
    @GuardedBy("mLock")
    private boolean mIsResuming;
    @GuardedBy("mLock")
    private int mShutdownPrepareTimeMs = MIN_MAX_GARAGE_MODE_DURATION_MS;
    @GuardedBy("mLock")
    private int mShutdownPollingIntervalMs = SHUTDOWN_POLLING_INTERVAL_MS;
    @GuardedBy("mLock")
    private boolean mRebootAfterGarageMode;
    @GuardedBy("mLock")
    private boolean mGarageModeShouldExitImmediately;
    private final boolean mDisableUserSwitchDuringResume;

    private final UserManager mUserManager;
    private final CarUserService mUserService;
    private final InitialUserSetter mInitialUserSetter;

    private final IVoiceInteractionManagerService mVoiceInteractionManagerService;

    private final WifiManager mWifiManager;
    private final AtomicFile mWifiStateFile;

    // TODO:  Make this OEM configurable.
    private static final int SHUTDOWN_POLLING_INTERVAL_MS = 2000;
    private static final int SHUTDOWN_EXTEND_MAX_MS = 5000;

    // maxGarageModeRunningDurationInSecs should be equal or greater than this. 15 min for now.
    private static final int MIN_MAX_GARAGE_MODE_DURATION_MS = 15 * 60 * 1000;

    // in secs
    private static final String PROP_MAX_GARAGE_MODE_DURATION_OVERRIDE =
            "android.car.garagemodeduration";

    // This is a temp work-around to reduce user switching delay after wake-up.
    private final boolean mSwitchGuestUserBeforeSleep;

    // CPMS tries to enter Suspend to RAM within the duration specified at
    // mMaxSuspendWaitDurationMs. The default max duration is MAX_SUSPEND_WAIT_DURATION, and can be
    // overridden by setting config_maxSuspendWaitDuration in an overrlay resource.
    // The valid range is MIN_SUSPEND_WAIT_DRATION to MAX_SUSPEND_WAIT_DURATION.
    private final long mMaxSuspendWaitDurationMs;

    private class PowerManagerCallbackList extends RemoteCallbackList<ICarPowerStateListener> {
        /**
         * Old version of {@link #onCallbackDied(E, Object)} that
         * does not provide a cookie.
         */
        @Override
        public void onCallbackDied(ICarPowerStateListener listener) {
            Slog.i(TAG, "binderDied " + listener.asBinder());
            CarPowerManagementService.this.doUnregisterListener(listener);
        }
    }

    public CarPowerManagementService(Context context, PowerHalService powerHal,
            SystemInterface systemInterface, CarUserService carUserService) {
        this(context, context.getResources(), powerHal, systemInterface, UserManager.get(context),
                carUserService, new InitialUserSetter(context,
                        (u) -> carUserService.setInitialUser(u),
                        context.getString(R.string.default_guest_name)),
                IVoiceInteractionManagerService.Stub.asInterface(
                        ServiceManager.getService(Context.VOICE_INTERACTION_MANAGER_SERVICE)));
    }

    @VisibleForTesting
    public CarPowerManagementService(Context context, Resources resources, PowerHalService powerHal,
            SystemInterface systemInterface, UserManager userManager, CarUserService carUserService,
            InitialUserSetter initialUserSetter,
            IVoiceInteractionManagerService voiceInteractionService) {
        mContext = context;
        mHal = powerHal;
        mSystemInterface = systemInterface;
        mUserManager = userManager;
        mDisableUserSwitchDuringResume = resources
                .getBoolean(R.bool.config_disableUserSwitchDuringResume);
        mShutdownPrepareTimeMs = resources.getInteger(
                R.integer.maxGarageModeRunningDurationInSecs) * 1000;
        mSwitchGuestUserBeforeSleep = resources.getBoolean(
                R.bool.config_switchGuestUserBeforeGoingSleep);
        if (mShutdownPrepareTimeMs < MIN_MAX_GARAGE_MODE_DURATION_MS) {
            Slog.w(TAG,
                    "maxGarageModeRunningDurationInSecs smaller than minimum required, resource:"
                    + mShutdownPrepareTimeMs + "(ms) while should exceed:"
                    +  MIN_MAX_GARAGE_MODE_DURATION_MS + "(ms), Ignore resource.");
            mShutdownPrepareTimeMs = MIN_MAX_GARAGE_MODE_DURATION_MS;
        }
        mUserService = carUserService;
        mInitialUserSetter = initialUserSetter;
        mVoiceInteractionManagerService = voiceInteractionService;
        mWifiManager = context.getSystemService(WifiManager.class);
        mWifiStateFile = new AtomicFile(
                new File(mSystemInterface.getSystemCarDir(), WIFI_STATE_FILENAME));
        mMaxSuspendWaitDurationMs = Math.max(MIN_SUSPEND_WAIT_DURATION_MS,
                Math.min(getMaxSuspendWaitDurationConfig(), MAX_SUSPEND_WAIT_DURATION_MS));
    }

    @VisibleForTesting
    public void setShutdownTimersForTest(int pollingIntervalMs, int shutdownTimeoutMs) {
        // Override timers to keep testing time short
        // Passing in '0' resets the value to the default
        synchronized (mLock) {
            mShutdownPollingIntervalMs =
                    (pollingIntervalMs == 0) ? SHUTDOWN_POLLING_INTERVAL_MS : pollingIntervalMs;
            mShutdownPrepareTimeMs =
                    (shutdownTimeoutMs == 0) ? SHUTDOWN_EXTEND_MAX_MS : shutdownTimeoutMs;
        }
    }

    @VisibleForTesting
    protected HandlerThread getHandlerThread() {
        return mHandlerThread;
    }

    @Override
    public void init() {
        mHal.setListener(this);
        if (mHal.isPowerStateSupported()) {
            // Initialize CPMS in WAIT_FOR_VHAL state
            onApPowerStateChange(CpmsState.WAIT_FOR_VHAL, CarPowerStateListener.WAIT_FOR_VHAL);
        } else {
            Slog.w(TAG, "Vehicle hal does not support power state yet.");
            onApPowerStateChange(CpmsState.ON, CarPowerStateListener.ON);
        }
        mSystemInterface.startDisplayStateMonitoring(this);
    }

    @Override
    public void release() {
        synchronized (mLock) {
            releaseTimerLocked();
            mCurrentState = null;
            mHandler.cancelAll();
            mListenersWeAreWaitingFor.clear();
        }
        mSystemInterface.stopDisplayStateMonitoring();
        mPowerManagerListeners.kill();
        mSystemInterface.releaseAllWakeLocks();
    }

    @Override
    public void dump(PrintWriter writer) {
        synchronized (mLock) {
            writer.println("*PowerManagementService*");
            // TODO: split it in multiple lines
            // TODO: lock only what's needed
            writer.print("mCurrentState:" + mCurrentState);
            writer.print(",mProcessingStartTime:" + mProcessingStartTime);
            writer.print(",mLastSleepEntryTime:" + mLastSleepEntryTime);
            writer.print(",mNextWakeupSec:" + mNextWakeupSec);
            writer.print(",mShutdownOnNextSuspend:" + mShutdownOnNextSuspend);
            writer.print(",mShutdownOnFinish:" + mShutdownOnFinish);
            writer.print(",mShutdownPollingIntervalMs:" + mShutdownPollingIntervalMs);
            writer.print(",mShutdownPrepareTimeMs:" + mShutdownPrepareTimeMs);
            writer.print(",mDisableUserSwitchDuringResume:" + mDisableUserSwitchDuringResume);
            writer.println(",mRebootAfterGarageMode:" + mRebootAfterGarageMode);
            writer.println("mSwitchGuestUserBeforeSleep:" + mSwitchGuestUserBeforeSleep);
            writer.print("mMaxSuspendWaitDurationMs:" + mMaxSuspendWaitDurationMs);
            writer.println(", config_maxSuspendWaitDuration:" + getMaxSuspendWaitDurationConfig());
        }
        mInitialUserSetter.dump(writer);
    }

    @Override
    public void onApPowerStateChange(PowerState state) {
        synchronized (mLock) {
            mPendingPowerStates.addFirst(new CpmsState(state));
            mLock.notify();
        }
        mHandler.handlePowerStateChange();
    }

    @VisibleForTesting
    void setStateForTesting(boolean isBooting, boolean isResuming) {
        synchronized (mLock) {
            Slog.d(TAG, "setStateForTesting():"
                    + " booting(" + mIsBooting + ">" + isBooting + ")"
                    + " resuming(" + mIsResuming + ">" + isResuming + ")");
            mIsBooting = isBooting;
            mIsResuming = isResuming;
        }
    }

    /**
     * Initiate state change from CPMS directly.
     */
    private void onApPowerStateChange(int apState, int carPowerStateListenerState) {
        CpmsState newState = new CpmsState(apState, carPowerStateListenerState);
        synchronized (mLock) {
            mPendingPowerStates.addFirst(newState);
            mLock.notify();
        }
        mHandler.handlePowerStateChange();
    }

    private void doHandlePowerStateChange() {
        CpmsState state;
        synchronized (mLock) {
            state = mPendingPowerStates.peekFirst();
            mPendingPowerStates.clear();
            if (state == null) {
                Slog.e(TAG, "Null power state was requested");
                return;
            }
            Slog.i(TAG, "doHandlePowerStateChange: newState=" + state.name());
            if (!needPowerStateChangeLocked(state)) {
                return;
            }
            // now real power change happens. Whatever was queued before should be all cancelled.
            releaseTimerLocked();
        }
        mHandler.cancelProcessingComplete();
        Slog.i(TAG, "setCurrentState " + state.toString());
        CarStatsLogHelper.logPowerState(state.mState);
        mCurrentState = state;
        switch (state.mState) {
            case CpmsState.WAIT_FOR_VHAL:
                handleWaitForVhal(state);
                break;
            case CpmsState.ON:
                handleOn();
                break;
            case CpmsState.SHUTDOWN_PREPARE:
                handleShutdownPrepare(state);
                break;
            case CpmsState.SIMULATE_SLEEP:
                simulateShutdownPrepare();
                break;
            case CpmsState.WAIT_FOR_FINISH:
                handleWaitForFinish(state);
                break;
            case CpmsState.SUSPEND:
                // Received FINISH from VHAL
                handleFinish();
                break;
            default:
                // Illegal state
                // TODO:  Throw exception?
                break;
        }
    }

    private void handleWaitForVhal(CpmsState state) {
        int carPowerStateListenerState = state.mCarPowerStateListenerState;
        sendPowerManagerEvent(carPowerStateListenerState);
        // Inspect CarPowerStateListenerState to decide which message to send via VHAL
        switch (carPowerStateListenerState) {
            case CarPowerStateListener.WAIT_FOR_VHAL:
                mHal.sendWaitForVhal();
                break;
            case CarPowerStateListener.SHUTDOWN_CANCELLED:
                mShutdownOnNextSuspend = false; // This cancels the "NextSuspend"
                mHal.sendShutdownCancel();
                break;
            case CarPowerStateListener.SUSPEND_EXIT:
                mHal.sendSleepExit();
                break;
        }
        restoreWifi();
    }

    private void updateCarUserNoticeServiceIfNecessary() {
        try {
            int currentUserId = ActivityManager.getCurrentUser();
            UserInfo currentUserInfo = mUserManager.getUserInfo(currentUserId);
            CarUserNoticeService carUserNoticeService =
                    CarLocalServices.getService(CarUserNoticeService.class);
            if (currentUserInfo != null && currentUserInfo.isGuest()
                    && carUserNoticeService != null) {
                Slog.i(TAG, "Car user notice service will ignore all messages before user switch.");
                Intent intent = new Intent();
                intent.setComponent(new ComponentName(mContext.getPackageName(),
                        ContinuousBlankActivity.class.getName()));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mContext.startActivityAsUser(intent, UserHandle.CURRENT);
                carUserNoticeService.ignoreUserNotice(currentUserId);
            }
        } catch (Exception e) {
            Slog.w(TAG, "Cannot ignore user notice for current user", e);
        }
    }

    private void handleOn() {
        // If current user is a Guest User, we want to inform CarUserNoticeService not to show
        // notice for current user, and show user notice only for the target user.
        if (!mSwitchGuestUserBeforeSleep) {
            updateCarUserNoticeServiceIfNecessary();
        }

        // Some OEMs have their own user-switching logic, which may not be coordinated with this
        // code. To avoid contention, we don't switch users when we coming alive. The OEM's code
        // should do the switch.
        boolean allowUserSwitch = true;
        synchronized (mLock) {
            if (mIsBooting) {
                // The system is booting, so don't switch users
                allowUserSwitch = false;
                mIsBooting = false;
                mIsResuming = false;
                Slog.i(TAG, "User switch disallowed while booting");
            } else {
                // The system is resuming after a suspension. Optionally disable user switching.
                allowUserSwitch = !mDisableUserSwitchDuringResume;
                mIsBooting = false;
                mIsResuming = false;
                if (!allowUserSwitch) {
                    Slog.i(TAG, "User switch disallowed while resuming");
                }
            }
        }

        mSystemInterface.setDisplayState(true);
        sendPowerManagerEvent(CarPowerStateListener.ON);

        mHal.sendOn();

        try {
            switchUserOnResumeIfNecessary(allowUserSwitch);
        } catch (Exception e) {
            Slog.e(TAG, "Could not switch user on resume", e);
        }

        setVoiceInteractionDisabled(false);
    }

    @VisibleForTesting // Ideally it should not be exposed, but it speeds up the unit tests
    void switchUserOnResumeIfNecessary(boolean allowSwitching) {
        Slog.d(TAG, "switchUserOnResumeIfNecessary(): allowSwitching=" + allowSwitching
                + ", mSwitchGuestUserBeforeSleep=" + mSwitchGuestUserBeforeSleep);
        if (!allowSwitching) {
            if (mSwitchGuestUserBeforeSleep) { // already handled
                return;
            }
            switchToNewGuestIfNecessary();
            return;
        }

        if (CarProperties.user_hal_enabled().orElse(false) && mUserService.isUserHalSupported()) {
            switchUserOnResumeIfNecessaryUsingHal();
            return;
        }

        executeDefaultInitialUserBehavior(!mSwitchGuestUserBeforeSleep);
    }

    private void executeDefaultInitialUserBehavior(boolean replaceGuest) {
        mInitialUserSetter.set(newInitialUserInfoBuilder(InitialUserSetter.TYPE_DEFAULT_BEHAVIOR)
                .setReplaceGuest(replaceGuest)
                .build());
    }

    /**
     * Replaces the current user if it's a guest.
     */
    private void switchToNewGuestIfNecessary() {
        int currentUserId = ActivityManager.getCurrentUser();
        UserInfo currentUser = mUserManager.getUserInfo(currentUserId);

        if (!mInitialUserSetter.canReplaceGuestUser(currentUser)) return; // Not a guest

        mInitialUserSetter.set(newInitialUserInfoBuilder(InitialUserSetter.TYPE_REPLACE_GUEST)
                .build());
    }

    private void switchUser(@UserIdInt int userId, boolean replaceGuest) {
        mInitialUserSetter.set(newInitialUserInfoBuilder(InitialUserSetter.TYPE_SWITCH)
                .setSwitchUserId(userId).setReplaceGuest(replaceGuest).build());
    }

    private InitialUserSetter.Builder newInitialUserInfoBuilder(@InitialUserInfoType int type) {
        return new InitialUserSetter.Builder(type)
                .setSupportsOverrideUserIdProperty(!mUserService.isUserHalSupported());
    }

    /**
     * Tells Garage Mode if it should run normally, or just
     * exit immediately without indicating 'idle'
     * @return True if no idle jobs should be run
     * @hide
     */
    public boolean garageModeShouldExitImmediately() {
        synchronized (mLock) {
            return mGarageModeShouldExitImmediately;
        }
    }

    /**
     * Switches the initial user by calling the User HAL to define the behavior.
     */
    private void switchUserOnResumeIfNecessaryUsingHal() {
        Slog.i(TAG, "Using User HAL to define initial user behavior");
        mUserService.getInitialUserInfo(InitialUserInfoRequestType.RESUME, (status, response) -> {
            switch (status) {
                case HalCallback.STATUS_HAL_RESPONSE_TIMEOUT:
                case HalCallback.STATUS_HAL_SET_TIMEOUT:
                    switchUserOnResumeUserHalFallback("timeout");
                    return;
                case HalCallback.STATUS_CONCURRENT_OPERATION:
                    switchUserOnResumeUserHalFallback("concurrent call");
                    return;
                case HalCallback.STATUS_WRONG_HAL_RESPONSE:
                    switchUserOnResumeUserHalFallback("wrong response");
                    return;
                case HalCallback.STATUS_HAL_NOT_SUPPORTED:
                    switchUserOnResumeUserHalFallback("Hal not supported");
                    return;
                case HalCallback.STATUS_OK:
                    if (response == null) {
                        switchUserOnResumeUserHalFallback("no response");
                        return;
                    }
                    boolean replaceGuest = !mSwitchGuestUserBeforeSleep;
                    switch (response.action) {
                        case InitialUserInfoResponseAction.DEFAULT:
                            Slog.i(TAG, "HAL requested default initial user behavior");
                            executeDefaultInitialUserBehavior(replaceGuest);
                            return;
                        case InitialUserInfoResponseAction.SWITCH:
                            int userId = response.userToSwitchOrCreate.userId;
                            Slog.i(TAG, "HAL requested switch to user " + userId);
                            // If guest was replaced on shutdown, it doesn't need to be replaced
                            // again
                            switchUser(userId, replaceGuest);
                            return;
                        case InitialUserInfoResponseAction.CREATE:
                            int halFlags = response.userToSwitchOrCreate.flags;
                            String name = response.userNameToCreate;
                            Slog.i(TAG, "HAL requested new user (name="
                                    + UserHelper.safeName(name) + ", flags="
                                    + UserHalHelper.userFlagsToString(halFlags) + ")");
                            mInitialUserSetter
                                    .set(newInitialUserInfoBuilder(InitialUserSetter.TYPE_CREATE)
                                            .setNewUserName(name)
                                            .setNewUserFlags(halFlags)
                                            .build());
                            return;
                        default:
                            switchUserOnResumeUserHalFallback(
                                    "invalid response action: " + response.action);
                            return;
                    }
                default:
                    switchUserOnResumeUserHalFallback("invalid status: " + status);
            }
        });
    }

    /**
     * Switches the initial user directly when the User HAL call failed.
     */
    private void switchUserOnResumeUserHalFallback(String reason) {
        Slog.w(TAG, "Failed to set initial user based on User Hal (" + reason
                + "); falling back to default behavior");
        executeDefaultInitialUserBehavior(!mSwitchGuestUserBeforeSleep);
    }

    private void handleShutdownPrepare(CpmsState newState) {
        setVoiceInteractionDisabled(true);
        mSystemInterface.setDisplayState(false);
        // Shutdown on finish if the system doesn't support deep sleep or doesn't allow it.
        synchronized (mLock) {
            mShutdownOnFinish = mShutdownOnNextSuspend
                    || !mHal.isDeepSleepAllowed()
                    || !mSystemInterface.isSystemSupportingDeepSleep()
                    || !newState.mCanSleep;
            mGarageModeShouldExitImmediately = !newState.mCanPostpone;
        }
        Slog.i(TAG,
                (newState.mCanPostpone
                ? "starting shutdown prepare with Garage Mode"
                        : "starting shutdown prepare without Garage Mode"));
        sendPowerManagerEvent(CarPowerStateListener.SHUTDOWN_PREPARE);
        mHal.sendShutdownPrepare();
        doHandlePreprocessing();
    }

    // Simulate system shutdown to Deep Sleep
    private void simulateShutdownPrepare() {
        mSystemInterface.setDisplayState(false);
        Slog.i(TAG, "starting shutdown prepare");
        sendPowerManagerEvent(CarPowerStateListener.SHUTDOWN_PREPARE);
        mHal.sendShutdownPrepare();
        doHandlePreprocessing();
    }

    private void handleWaitForFinish(CpmsState state) {
        sendPowerManagerEvent(state.mCarPowerStateListenerState);
        int wakeupSec;
        synchronized (mLock) {
            // If we're shutting down immediately, don't schedule
            // a wakeup time.
            wakeupSec = mGarageModeShouldExitImmediately ? 0 : mNextWakeupSec;
        }
        switch (state.mCarPowerStateListenerState) {
            case CarPowerStateListener.SUSPEND_ENTER:
                mHal.sendSleepEntry(wakeupSec);
                break;
            case CarPowerStateListener.SHUTDOWN_ENTER:
                mHal.sendShutdownStart(wakeupSec);
                break;
        }
    }

    private void handleFinish() {
        boolean simulatedMode;
        synchronized (mSimulationWaitObject) {
            simulatedMode = mInSimulatedDeepSleepMode;
        }
        boolean mustShutDown;
        boolean forceReboot;
        synchronized (mLock) {
            mustShutDown = mShutdownOnFinish && !simulatedMode;
            forceReboot = mRebootAfterGarageMode;
            mRebootAfterGarageMode = false;
        }
        if (forceReboot) {
            PowerManager powerManager = mContext.getSystemService(PowerManager.class);
            if (powerManager == null) {
                Slog.wtf(TAG, "No PowerManager. Cannot reboot.");
            } else {
                Slog.i(TAG, "GarageMode has completed. Forcing reboot.");
                powerManager.reboot("GarageModeReboot");
                throw new AssertionError("Should not return from PowerManager.reboot()");
            }
        }
        setVoiceInteractionDisabled(true);

        // To make Kernel implementation simpler when going into sleep.
        disableWifi();

        if (mustShutDown) {
            // shutdown HU
            mSystemInterface.shutdown();
        } else {
            doHandleDeepSleep(simulatedMode);
        }
        mShutdownOnNextSuspend = false;
    }

    private void setVoiceInteractionDisabled(boolean disabled) {
        try {
            mVoiceInteractionManagerService.setDisabled(disabled);
        } catch (RemoteException e) {
            Slog.w(TAG, "setVoiceIntefactionDisabled(" + disabled + ") failed", e);
        }
    }

    private void restoreWifi() {
        boolean needToRestore = readWifiModifiedState();
        if (needToRestore) {
            if (!mWifiManager.isWifiEnabled()) {
                Slog.i(TAG, "Wifi has been enabled to restore the last setting");
                mWifiManager.setWifiEnabled(true);
            }
            // Update the persistent data as wifi is not modified by car framework.
            saveWifiModifiedState(false);
        }
    }

    private void disableWifi() {
        boolean wifiEnabled = mWifiManager.isWifiEnabled();
        boolean wifiModifiedState = readWifiModifiedState();
        if (wifiEnabled != wifiModifiedState) {
            saveWifiModifiedState(wifiEnabled);
        }
        if (!wifiEnabled) return;

        mWifiManager.setWifiEnabled(false);
        wifiEnabled = mWifiManager.isWifiEnabled();
        Slog.i(TAG, "Wifi has been disabled and the last setting was saved");
    }

    private void saveWifiModifiedState(boolean forciblyDisabled) {
        FileOutputStream fos;
        try {
            fos = mWifiStateFile.startWrite();
        } catch (IOException e) {
            Slog.e(TAG, "Cannot create " + WIFI_STATE_FILENAME, e);
            return;
        }

        try (BufferedWriter writer = new BufferedWriter(
                new OutputStreamWriter(fos, StandardCharsets.UTF_8))) {
            writer.write(forciblyDisabled ? WIFI_STATE_MODIFIED : WIFI_STATE_ORIGINAL);
            writer.newLine();
            writer.flush();
            mWifiStateFile.finishWrite(fos);
        } catch (IOException e) {
            mWifiStateFile.failWrite(fos);
            Slog.e(TAG, "Writing " + WIFI_STATE_FILENAME + " failed", e);
        }
    }

    private boolean readWifiModifiedState() {
        boolean needToRestore = false;
        boolean invalidState = false;

        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(mWifiStateFile.openRead(), StandardCharsets.UTF_8))) {
            String line = reader.readLine();
            if (line == null) {
                needToRestore = false;
                invalidState = true;
            } else {
                line = line.trim();
                needToRestore = WIFI_STATE_MODIFIED.equals(line);
                invalidState = !(needToRestore || WIFI_STATE_ORIGINAL.equals(line));
            }
        } catch (IOException e) {
            // If a file named wifi_state doesn't exist, we will not modify Wifi at system start.
            Slog.w(TAG, "Failed to read " + WIFI_STATE_FILENAME + ": " + e);
            return false;
        }
        if (invalidState) {
            mWifiStateFile.delete();
        }

        return needToRestore;
    }

    @GuardedBy("mLock")
    private void releaseTimerLocked() {
        if (mTimer != null) {
            mTimer.cancel();
        }
        mTimer = null;
        mTimerActive = false;
    }

    private void doHandlePreprocessing() {
        int intervalMs;
        int pollingCount;
        synchronized (mLock) {
            intervalMs = mShutdownPollingIntervalMs;
            pollingCount = (mShutdownPrepareTimeMs / mShutdownPollingIntervalMs) + 1;
        }
        if (Build.IS_USERDEBUG || Build.IS_ENG) {
            int shutdownPrepareTimeOverrideInSecs =
                    SystemProperties.getInt(PROP_MAX_GARAGE_MODE_DURATION_OVERRIDE, -1);
            if (shutdownPrepareTimeOverrideInSecs >= 0) {
                pollingCount =
                        (shutdownPrepareTimeOverrideInSecs * 1000 / intervalMs)
                                + 1;
                Slog.i(TAG, "Garage mode duration overridden secs:"
                        + shutdownPrepareTimeOverrideInSecs);
            }
        }
        Slog.i(TAG, "processing before shutdown expected for: "
                + mShutdownPrepareTimeMs + " ms, adding polling:" + pollingCount);
        synchronized (mLock) {
            mProcessingStartTime = SystemClock.elapsedRealtime();
            releaseTimerLocked();
            mTimer = new Timer();
            mTimerActive = true;
            mTimer.scheduleAtFixedRate(
                    new ShutdownProcessingTimerTask(pollingCount),
                    0 /*delay*/,
                    intervalMs);
        }
        if (mSwitchGuestUserBeforeSleep) {
            switchToNewGuestIfNecessary();
        }
    }

    private void sendPowerManagerEvent(int newState) {
        // Broadcast to the listeners that do not signal completion
        notifyListeners(mPowerManagerListeners, newState);

        // SHUTDOWN_PREPARE is the only state where we need
        // to maintain callbacks from listener components.
        boolean allowCompletion = (newState == CarPowerStateListener.SHUTDOWN_PREPARE);

        // Fully populate mListenersWeAreWaitingFor before calling any onStateChanged()
        // for the listeners that signal completion.
        // Otherwise, if the first listener calls finish() synchronously, we will
        // see the list go empty and we will think that we are done.
        boolean haveSomeCompleters = false;
        PowerManagerCallbackList completingListeners = new PowerManagerCallbackList();
        synchronized (mLock) {
            mListenersWeAreWaitingFor.clear();
            int idx = mPowerManagerListenersWithCompletion.beginBroadcast();
            while (idx-- > 0) {
                ICarPowerStateListener listener =
                        mPowerManagerListenersWithCompletion.getBroadcastItem(idx);
                completingListeners.register(listener);
                if (allowCompletion) {
                    mListenersWeAreWaitingFor.add(listener.asBinder());
                    haveSomeCompleters = true;
                }
            }
            mPowerManagerListenersWithCompletion.finishBroadcast();
        }
        // Broadcast to the listeners that DO signal completion
        notifyListeners(completingListeners, newState);

        if (allowCompletion && !haveSomeCompleters) {
            // No jobs need to signal completion. So we are now complete.
            signalComplete();
        }
    }

    private void notifyListeners(PowerManagerCallbackList listenerList, int newState) {
        int idx = listenerList.beginBroadcast();
        while (idx-- > 0) {
            ICarPowerStateListener listener = listenerList.getBroadcastItem(idx);
            try {
                listener.onStateChanged(newState);
            } catch (RemoteException e) {
                // It's likely the connection snapped. Let binder death handle the situation.
                Slog.e(TAG, "onStateChanged() call failed", e);
            }
        }
        listenerList.finishBroadcast();
    }

    private void doHandleDeepSleep(boolean simulatedMode) {
        // keep holding partial wakelock to prevent entering sleep before enterDeepSleep call
        // enterDeepSleep should force sleep entry even if wake lock is kept.
        mSystemInterface.switchToPartialWakeLock();
        mHandler.cancelProcessingComplete();
        synchronized (mLock) {
            mLastSleepEntryTime = SystemClock.elapsedRealtime();
        }
        int nextListenerState;
        if (simulatedMode) {
            simulateSleepByWaiting();
            nextListenerState = CarPowerStateListener.SHUTDOWN_CANCELLED;
        } else {
            boolean sleepSucceeded = suspendWithRetries();
            if (!sleepSucceeded) {
                // Suspend failed and we shut down instead.
                // We either won't get here at all or we will power off very soon.
                return;
            }
            // We suspended and have now resumed
            nextListenerState = CarPowerStateListener.SUSPEND_EXIT;
        }
        synchronized (mLock) {
            mIsResuming = true;
            // Any wakeup time from before is no longer valid.
            mNextWakeupSec = 0;
        }
        Slog.i(TAG, "Resuming after suspending");
        mSystemInterface.refreshDisplayBrightness();
        onApPowerStateChange(CpmsState.WAIT_FOR_VHAL, nextListenerState);
    }

    private boolean needPowerStateChangeLocked(@NonNull CpmsState newState) {
        if (mCurrentState == null) {
            return true;
        } else if (mCurrentState.equals(newState)) {
            Slog.d(TAG, "Requested state is already in effect: " + newState.name());
            return false;
        }

        // The following switch/case enforces the allowed state transitions.
        boolean transitionAllowed = false;
        switch (mCurrentState.mState) {
            case CpmsState.WAIT_FOR_VHAL:
                transitionAllowed = (newState.mState == CpmsState.ON)
                    || (newState.mState == CpmsState.SHUTDOWN_PREPARE);
                break;
            case CpmsState.SUSPEND:
                transitionAllowed =  newState.mState == CpmsState.WAIT_FOR_VHAL;
                break;
            case CpmsState.ON:
                transitionAllowed = (newState.mState == CpmsState.SHUTDOWN_PREPARE)
                    || (newState.mState == CpmsState.SIMULATE_SLEEP);
                break;
            case CpmsState.SHUTDOWN_PREPARE:
                // If VHAL sends SHUTDOWN_IMMEDIATELY or SLEEP_IMMEDIATELY while in
                // SHUTDOWN_PREPARE state, do it.
                transitionAllowed =
                        ((newState.mState == CpmsState.SHUTDOWN_PREPARE) && !newState.mCanPostpone)
                                || (newState.mState == CpmsState.WAIT_FOR_FINISH)
                                || (newState.mState == CpmsState.WAIT_FOR_VHAL);
                break;
            case CpmsState.SIMULATE_SLEEP:
                transitionAllowed = true;
                break;
            case CpmsState.WAIT_FOR_FINISH:
                transitionAllowed = (newState.mState == CpmsState.SUSPEND
                        || newState.mState == CpmsState.WAIT_FOR_VHAL);
                break;
            default:
                Slog.e(TAG, "Unexpected current state:  currentState="
                        + mCurrentState.name() + ", newState=" + newState.name());
                transitionAllowed = true;
        }
        if (!transitionAllowed) {
            Slog.e(TAG, "Requested power transition is not allowed: "
                    + mCurrentState.name() + " --> " + newState.name());
        }
        return transitionAllowed;
    }

    private void doHandleProcessingComplete() {
        int listenerState;
        synchronized (mLock) {
            releaseTimerLocked();
            if (!mShutdownOnFinish && mLastSleepEntryTime > mProcessingStartTime) {
                // entered sleep after processing start. So this could be duplicate request.
                Slog.w(TAG, "Duplicate sleep entry request, ignore");
                return;
            }
            listenerState = mShutdownOnFinish
                    ? CarPowerStateListener.SHUTDOWN_ENTER : CarPowerStateListener.SUSPEND_ENTER;
        }

        onApPowerStateChange(CpmsState.WAIT_FOR_FINISH, listenerState);
    }

    @Override
    public void onDisplayBrightnessChange(int brightness) {
        mHandler.handleDisplayBrightnessChange(brightness);
    }

    private void doHandleDisplayBrightnessChange(int brightness) {
        mSystemInterface.setDisplayBrightness(brightness);
    }

    private void doHandleMainDisplayStateChange(boolean on) {
        Slog.w(TAG, "Unimplemented:  doHandleMainDisplayStateChange() - on = " + on);
    }

    public void handleMainDisplayChanged(boolean on) {
        mHandler.handleMainDisplayStateChange(on);
    }

    /**
     * Send display brightness to VHAL.
     * @param brightness value 0-100%
     */
    public void sendDisplayBrightness(int brightness) {
        mHal.sendDisplayBrightness(brightness);
    }

    /**
     * Get the PowerHandler that we use to change power states
     */
    public Handler getHandler() {
        return mHandler;

    }

    // Binder interface for general use.
    // The listener is not required (or allowed) to call finished().
    @Override
    public void registerListener(ICarPowerStateListener listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        mPowerManagerListeners.register(listener);
    }

    // Binder interface for Car services only.
    // After the listener completes its processing, it must call finished().
    @Override
    public void registerListenerWithCompletion(ICarPowerStateListener listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        ICarImpl.assertCallingFromSystemProcessOrSelf();

        mPowerManagerListenersWithCompletion.register(listener);
        // TODO: Need to send current state to newly registered listener? If so, need to handle
        //       completion for SHUTDOWN_PREPARE state
    }

    @Override
    public void unregisterListener(ICarPowerStateListener listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        doUnregisterListener(listener);
    }

    private void doUnregisterListener(ICarPowerStateListener listener) {
        mPowerManagerListeners.unregister(listener);
        boolean found = mPowerManagerListenersWithCompletion.unregister(listener);
        if (found) {
            // Remove this from the completion list (if it's there)
            finishedImpl(listener.asBinder());
        }
    }

    @Override
    public void requestShutdownOnNextSuspend() {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        synchronized (mLock) {
            mShutdownOnNextSuspend = true;
        }
    }

    @Override
    public void finished(ICarPowerStateListener listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        ICarImpl.assertCallingFromSystemProcessOrSelf();
        finishedImpl(listener.asBinder());
    }

    @Override
    public void scheduleNextWakeupTime(int seconds) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        if (seconds < 0) {
            Slog.w(TAG, "Next wake up time is negative. Ignoring!");
            return;
        }
        boolean timedWakeupAllowed = mHal.isTimedWakeupAllowed();
        synchronized (mLock) {
            if (!timedWakeupAllowed) {
                Slog.w(TAG, "Setting timed wakeups are disabled in HAL. Skipping");
                mNextWakeupSec = 0;
                return;
            }
            if (mNextWakeupSec == 0 || mNextWakeupSec > seconds) {
                // The new value is sooner than the old value. Take the new value.
                mNextWakeupSec = seconds;
            } else {
                Slog.d(TAG, "Tried to schedule next wake up, but already had shorter "
                        + "scheduled time");
            }
        }
    }

    @Override
    public int getPowerState() {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        synchronized (mLock) {
            return (mCurrentState == null) ? CarPowerStateListener.INVALID
                    : mCurrentState.mCarPowerStateListenerState;
        }
    }

    private void finishedImpl(IBinder binder) {
        boolean allAreComplete;
        synchronized (mLock) {
            mListenersWeAreWaitingFor.remove(binder);
            allAreComplete = mListenersWeAreWaitingFor.isEmpty();
        }
        if (allAreComplete) {
            signalComplete();
        }
    }

    private void signalComplete() {
        if (mCurrentState.mState == CpmsState.SHUTDOWN_PREPARE
                || mCurrentState.mState == CpmsState.SIMULATE_SLEEP) {
            PowerHandler powerHandler;
            // All apps are ready to shutdown/suspend.
            synchronized (mLock) {
                if (!mShutdownOnFinish) {
                    if (mLastSleepEntryTime > mProcessingStartTime
                            && mLastSleepEntryTime < SystemClock.elapsedRealtime()) {
                        Slog.i(TAG, "signalComplete: Already slept!");
                        return;
                    }
                }
                powerHandler = mHandler;
            }
            Slog.i(TAG, "Apps are finished, call handleProcessingComplete()");
            powerHandler.handleProcessingComplete();
        }
    }

    private static final class PowerHandler extends Handler {
        private static final String TAG = PowerHandler.class.getSimpleName();

        private final int MSG_POWER_STATE_CHANGE = 0;
        private final int MSG_DISPLAY_BRIGHTNESS_CHANGE = 1;
        private final int MSG_MAIN_DISPLAY_STATE_CHANGE = 2;
        private final int MSG_PROCESSING_COMPLETE = 3;

        // Do not handle this immediately but with some delay as there can be a race between
        // display off due to rear view camera and delivery to here.
        private final long MAIN_DISPLAY_EVENT_DELAY_MS = 500;

        private final WeakReference<CarPowerManagementService> mService;

        private PowerHandler(Looper looper, CarPowerManagementService service) {
            super(looper);
            mService = new WeakReference<CarPowerManagementService>(service);
        }

        private void handlePowerStateChange() {
            Message msg = obtainMessage(MSG_POWER_STATE_CHANGE);
            sendMessage(msg);
        }

        private void handleDisplayBrightnessChange(int brightness) {
            Message msg = obtainMessage(MSG_DISPLAY_BRIGHTNESS_CHANGE, brightness, 0);
            sendMessage(msg);
        }

        private void handleMainDisplayStateChange(boolean on) {
            removeMessages(MSG_MAIN_DISPLAY_STATE_CHANGE);
            Message msg = obtainMessage(MSG_MAIN_DISPLAY_STATE_CHANGE, Boolean.valueOf(on));
            sendMessageDelayed(msg, MAIN_DISPLAY_EVENT_DELAY_MS);
        }

        private void handleProcessingComplete() {
            removeMessages(MSG_PROCESSING_COMPLETE);
            Message msg = obtainMessage(MSG_PROCESSING_COMPLETE);
            sendMessage(msg);
        }

        private void cancelProcessingComplete() {
            removeMessages(MSG_PROCESSING_COMPLETE);
        }

        private void cancelAll() {
            removeMessages(MSG_POWER_STATE_CHANGE);
            removeMessages(MSG_DISPLAY_BRIGHTNESS_CHANGE);
            removeMessages(MSG_MAIN_DISPLAY_STATE_CHANGE);
            removeMessages(MSG_PROCESSING_COMPLETE);
        }

        @Override
        public void handleMessage(Message msg) {
            CarPowerManagementService service = mService.get();
            if (service == null) {
                Slog.i(TAG, "handleMessage null service");
                return;
            }
            switch (msg.what) {
                case MSG_POWER_STATE_CHANGE:
                    service.doHandlePowerStateChange();
                    break;
                case MSG_DISPLAY_BRIGHTNESS_CHANGE:
                    service.doHandleDisplayBrightnessChange(msg.arg1);
                    break;
                case MSG_MAIN_DISPLAY_STATE_CHANGE:
                    service.doHandleMainDisplayStateChange((Boolean) msg.obj);
                    break;
                case MSG_PROCESSING_COMPLETE:
                    service.doHandleProcessingComplete();
                    break;
            }
        }
    }

    private class ShutdownProcessingTimerTask extends TimerTask {
        private final int mExpirationCount;
        private int mCurrentCount;

        private ShutdownProcessingTimerTask(int expirationCount) {
            mExpirationCount = expirationCount;
            mCurrentCount = 0;
        }

        @Override
        public void run() {
            synchronized (mLock) {
                if (!mTimerActive) {
                    // Ignore timer expiration since we got cancelled
                    return;
                }
                mCurrentCount++;
                if (mCurrentCount > mExpirationCount) {
                    PowerHandler handler;
                    releaseTimerLocked();
                    handler = mHandler;
                    handler.handleProcessingComplete();
                } else {
                    mHal.sendShutdownPostpone(SHUTDOWN_EXTEND_MAX_MS);
                }
            }
        }
    }

    // Send the command to enter Suspend to RAM.
    // If the command is not successful, try again with an exponential back-off.
    // If it fails repeatedly, send the command to shut down.
    // If we decide to go to a different power state, abort this retry mechanism.
    // Returns true if we successfully suspended.
    private boolean suspendWithRetries() {
        long retryIntervalMs = INITIAL_SUSPEND_RETRY_INTERVAL_MS;
        long totalWaitDurationMs = 0;

        while (true) {
            Slog.i(TAG, "Entering Suspend to RAM");
            boolean suspendSucceeded = mSystemInterface.enterDeepSleep();
            if (suspendSucceeded) {
                return true;
            }
            if (totalWaitDurationMs >= mMaxSuspendWaitDurationMs) {
                break;
            }
            // We failed to suspend. Block the thread briefly and try again.
            synchronized (mLock) {
                if (mPendingPowerStates.isEmpty()) {
                    Slog.w(TAG, "Failed to Suspend; will retry after " + retryIntervalMs + "ms.");
                    try {
                        mLock.wait(retryIntervalMs);
                    } catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                    }
                    totalWaitDurationMs += retryIntervalMs;
                    retryIntervalMs = Math.min(retryIntervalMs * 2, MAX_RETRY_INTERVAL_MS);
                }
                // Check for a new power state now, before going around the loop again
                if (!mPendingPowerStates.isEmpty()) {
                    Slog.i(TAG, "Terminating the attempt to Suspend to RAM");
                    return false;
                }
            }
        }
        // Too many failures trying to suspend. Shut down.
        Slog.w(TAG, "Could not Suspend to RAM after " + totalWaitDurationMs
                + "ms long trial. Shutting down.");
        mSystemInterface.shutdown();
        return false;
    }

    private static class CpmsState {
        // NOTE: When modifying states below, make sure to update CarPowerStateChanged.State in
        //   frameworks/base/cmds/statsd/src/atoms.proto also.
        public static final int WAIT_FOR_VHAL = 0;
        public static final int ON = 1;
        public static final int SHUTDOWN_PREPARE = 2;
        public static final int WAIT_FOR_FINISH = 3;
        public static final int SUSPEND = 4;
        public static final int SIMULATE_SLEEP = 5;

        /* Config values from AP_POWER_STATE_REQ */
        public final boolean mCanPostpone;
        public final boolean mCanSleep;
        /* Message sent to CarPowerStateListener in response to this state */
        public final int mCarPowerStateListenerState;
        /* One of the above state variables */
        public final int mState;

        /**
          * This constructor takes a PowerHalService.PowerState object and creates the corresponding
          * CPMS state from it.
          */
        CpmsState(PowerState halPowerState) {
            switch (halPowerState.mState) {
                case VehicleApPowerStateReq.ON:
                    this.mCanPostpone = false;
                    this.mCanSleep = false;
                    this.mCarPowerStateListenerState = cpmsStateToPowerStateListenerState(ON);
                    this.mState = ON;
                    break;
                case VehicleApPowerStateReq.SHUTDOWN_PREPARE:
                    this.mCanPostpone = halPowerState.canPostponeShutdown();
                    this.mCanSleep = halPowerState.canEnterDeepSleep();
                    this.mCarPowerStateListenerState = cpmsStateToPowerStateListenerState(
                            SHUTDOWN_PREPARE);
                    this.mState = SHUTDOWN_PREPARE;
                    break;
                case VehicleApPowerStateReq.CANCEL_SHUTDOWN:
                    this.mCanPostpone = false;
                    this.mCanSleep = false;
                    this.mCarPowerStateListenerState = CarPowerStateListener.SHUTDOWN_CANCELLED;
                    this.mState = WAIT_FOR_VHAL;
                    break;
                case VehicleApPowerStateReq.FINISHED:
                    this.mCanPostpone = false;
                    this.mCanSleep = false;
                    this.mCarPowerStateListenerState = cpmsStateToPowerStateListenerState(SUSPEND);
                    this.mState = SUSPEND;
                    break;
                default:
                    // Illegal state from PowerState.  Throw an exception?
                    this.mCanPostpone = false;
                    this.mCanSleep = false;
                    this.mCarPowerStateListenerState = 0;
                    this.mState = 0;
                    break;
            }
        }

        CpmsState(int state) {
            this(state, cpmsStateToPowerStateListenerState(state));
        }

        CpmsState(int state, int carPowerStateListenerState) {
            this.mCanPostpone = (state == SIMULATE_SLEEP);
            this.mCanSleep = (state == SIMULATE_SLEEP);
            this.mCarPowerStateListenerState = carPowerStateListenerState;
            this.mState = state;
        }

        public String name() {
            String baseName;
            switch(mState) {
                case WAIT_FOR_VHAL:     baseName = "WAIT_FOR_VHAL";    break;
                case ON:                baseName = "ON";               break;
                case SHUTDOWN_PREPARE:  baseName = "SHUTDOWN_PREPARE"; break;
                case WAIT_FOR_FINISH:   baseName = "WAIT_FOR_FINISH";  break;
                case SUSPEND:           baseName = "SUSPEND";          break;
                case SIMULATE_SLEEP:    baseName = "SIMULATE_SLEEP";   break;
                default:                baseName = "<unknown>";        break;
            }
            return baseName + "(" + mState + ")";
        }

        private static int cpmsStateToPowerStateListenerState(int state) {
            int powerStateListenerState = 0;

            // Set the CarPowerStateListenerState based on current state
            switch (state) {
                case ON:
                    powerStateListenerState = CarPowerStateListener.ON;
                    break;
                case SHUTDOWN_PREPARE:
                    powerStateListenerState = CarPowerStateListener.SHUTDOWN_PREPARE;
                    break;
                case SUSPEND:
                    powerStateListenerState = CarPowerStateListener.SUSPEND_ENTER;
                    break;
                case WAIT_FOR_VHAL:
                case WAIT_FOR_FINISH:
                default:
                    // Illegal state for this constructor.  Throw an exception?
                    break;
            }
            return powerStateListenerState;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof CpmsState)) {
                return false;
            }
            CpmsState that = (CpmsState) o;
            return this.mState == that.mState
                    && this.mCanSleep == that.mCanSleep
                    && this.mCanPostpone == that.mCanPostpone
                    && this.mCarPowerStateListenerState == that.mCarPowerStateListenerState;
        }

        @Override
        public String toString() {
            return "CpmsState canSleep:" + mCanSleep + ", canPostpone=" + mCanPostpone
                    + ", carPowerStateListenerState=" + mCarPowerStateListenerState
                    + ", CpmsState=" + this.name();
        }
    }

    /**
     * Resume after a manually-invoked suspend.
     * Invoked using "adb shell dumpsys activity service com.android.car resume".
     */
    public void forceSimulatedResume() {
        PowerHandler handler;
        synchronized (mLock) {
            // Cancel Garage Mode in case it's running
            mPendingPowerStates.addFirst(new CpmsState(CpmsState.WAIT_FOR_VHAL,
                                                       CarPowerStateListener.SHUTDOWN_CANCELLED));
            mLock.notify();
            handler = mHandler;
        }
        handler.handlePowerStateChange();

        synchronized (mSimulationWaitObject) {
            mWakeFromSimulatedSleep = true;
            mSimulationWaitObject.notify();
        }
    }

    /**
     * Manually enter simulated suspend (Deep Sleep) mode, trigging Garage mode.
     * If the parameter is 'true', reboot the system when Garage Mode completes.
     *
     * Invoked using "adb shell dumpsys activity service com.android.car suspend" or
     * "adb shell dumpsys activity service com.android.car garage-mode reboot".
     * This is similar to 'onApPowerStateChange()' except that it needs to create a CpmsState
     * that is not directly derived from a VehicleApPowerStateReq.
     */
    @VisibleForTesting
    void forceSuspendAndMaybeReboot(boolean shouldReboot) {
        synchronized (mSimulationWaitObject) {
            mInSimulatedDeepSleepMode = true;
            mWakeFromSimulatedSleep = false;
            mGarageModeShouldExitImmediately = false;
        }
        PowerHandler handler;
        synchronized (mLock) {
            mRebootAfterGarageMode = shouldReboot;
            mPendingPowerStates.addFirst(new CpmsState(CpmsState.SIMULATE_SLEEP,
                                                       CarPowerStateListener.SHUTDOWN_PREPARE));
            handler = mHandler;
        }
        handler.handlePowerStateChange();
    }

    /**
     * Powers off the device, considering the given options.
     *
     * <p>The final state can be "suspend-to-RAM" or "shutdown". Attempting to go to suspend-to-RAM
     * on devices which do not support it may lead to an unexpected system state.
     */
    public void powerOffFromCommand(boolean skipGarageMode, boolean shutdown) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_POWER);
        int param = 0;
        if (shutdown) {
            param = skipGarageMode ? VehicleApPowerStateShutdownParam.SHUTDOWN_IMMEDIATELY
                    : VehicleApPowerStateShutdownParam.SHUTDOWN_ONLY;
        } else {
            param = skipGarageMode ? VehicleApPowerStateShutdownParam.SLEEP_IMMEDIATELY
                    : VehicleApPowerStateShutdownParam.CAN_SLEEP;
        }
        PowerState state = new PowerState(VehicleApPowerStateReq.SHUTDOWN_PREPARE, param);
        synchronized (mLock) {
            mRebootAfterGarageMode = false;
            mPendingPowerStates.addFirst(new CpmsState(state));
            mLock.notify();
        }
        mHandler.handlePowerStateChange();
    }

    // In a real Deep Sleep, the hardware removes power from the CPU (but retains power
    // on the RAM). This puts the processor to sleep. Upon some external signal, power
    // is re-applied to the CPU, and processing resumes right where it left off.
    // We simulate this behavior by calling wait().
    // We continue from wait() when forceSimulatedResume() is called.
    private void simulateSleepByWaiting() {
        Slog.i(TAG, "Starting to simulate Deep Sleep by waiting");
        synchronized (mSimulationWaitObject) {
            while (!mWakeFromSimulatedSleep) {
                try {
                    mSimulationWaitObject.wait();
                } catch (InterruptedException ignored) {
                    Thread.currentThread().interrupt(); // Restore interrupted status
                }
            }
            mInSimulatedDeepSleepMode = false;
        }
        Slog.i(TAG, "Exit Deep Sleep simulation");
    }

    private int getMaxSuspendWaitDurationConfig() {
        return mContext.getResources().getInteger(R.integer.config_maxSuspendWaitDuration);
    }
}
