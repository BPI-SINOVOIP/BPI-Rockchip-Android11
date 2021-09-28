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

package com.android.car.user;

import static android.car.hardware.power.CarPowerManager.CarPowerStateListener;

import static com.android.car.CarLog.TAG_USER;

import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.car.CarNotConnectedException;
import android.car.hardware.power.CarPowerManager;
import android.car.settings.CarSettings;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.car.user.IUserNotice;
import android.car.user.IUserNoticeUI;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;
import android.view.IWindowManager;
import android.view.WindowManagerGlobal;

import androidx.annotation.VisibleForTesting;

import com.android.car.CarLocalServices;
import com.android.car.CarServiceBase;
import com.android.car.R;
import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;

/**
 * Service to show initial notice UI to user. It only launches it when setting is enabled and
 * it is up to notice UI (=Service) to dismiss itself upon user's request.
 *
 * <p>Conditions to show notice UI are:
 * <ol>
 *   <li>Cold boot
 *   <li><User switching
 *   <li>Car power state change to ON (happens in wakeup from suspend to RAM)
 * </ol>
 */
public final class CarUserNoticeService implements CarServiceBase {

    // Keyguard unlocking can be only polled as we cannot dismiss keyboard.
    // Polling will stop when keyguard is unlocked.
    private static final long KEYGUARD_POLLING_INTERVAL_MS = 100;

    private final Context mContext;

    // null means feature disabled.
    @Nullable
    private final Intent mServiceIntent;

    private final Handler mMainHandler;

    private final Object mLock = new Object();

    // This one records if there is a service bound. This will be cleared as soon as service is
    // unbound (=UI dismissed)
    @GuardedBy("mLock")
    private boolean mServiceBound = false;

    // This one represents if UI is shown for the current session. This should be kept until
    // next event to show UI comes up.
    @GuardedBy("mLock")
    private boolean mUiShown = false;

    @GuardedBy("mLock")
    @UserIdInt
    private int mUserId = UserHandle.USER_NULL;

    @GuardedBy("mLock")
    private CarPowerManager mCarPowerManager;

    @GuardedBy("mLock")
    private IUserNoticeUI mUiService;

    @GuardedBy("mLock")
    @UserIdInt
    private int mIgnoreUserId = UserHandle.USER_NULL;

    private final UserLifecycleListener mUserLifecycleListener = event -> {
        if (Log.isLoggable(TAG_USER, Log.DEBUG)) {
            Log.d(TAG_USER, "onEvent(" + event + ")");
        }
        if (CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING == event.getEventType()) {
            CarUserNoticeService.this.mMainHandler.post(() -> {
                stopUi(/* clearUiShown= */ true);
                synchronized (mLock) {
                   // This should be the only place to change user
                    mUserId = event.getUserId();
                }
                startNoticeUiIfNecessary();
            });
        }
    };

    private final CarPowerStateListener mPowerStateListener = new CarPowerStateListener() {
        @Override
        public void onStateChanged(int state) {
            if (state == CarPowerManager.CarPowerStateListener.SHUTDOWN_PREPARE) {
                mMainHandler.post(() -> stopUi(/* clearUiShown= */ true));
            } else if (state == CarPowerManager.CarPowerStateListener.ON) {
                // Only ON can be relied on as car can restart while in garage mode.
                mMainHandler.post(() -> startNoticeUiIfNecessary());
            }
        }
    };

    private final BroadcastReceiver mDisplayBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            // Runs in main thread, so do not use Handler.
            if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                if (isDisplayOn()) {
                    Log.i(TAG_USER, "SCREEN_OFF while display is already on");
                    return;
                }
                Log.i(TAG_USER, "Display off, stopping UI");
                stopUi(/* clearUiShown= */ true);
            } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                if (!isDisplayOn()) {
                    Log.i(TAG_USER, "SCREEN_ON while display is already off");
                    return;
                }
                Log.i(TAG_USER, "Display on, starting UI");
                startNoticeUiIfNecessary();
            }
        }
    };

    private final IUserNotice.Stub mIUserNotice = new IUserNotice.Stub() {
        @Override
        public void onDialogDismissed() {
            mMainHandler.post(() -> stopUi(/* clearUiShown= */ false));
        }
    };

    private final ServiceConnection mUiServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            synchronized (mLock) {
                if (!mServiceBound) {
                    // already unbound but passed due to timing. This should be just ignored.
                    return;
                }
            }
            IUserNoticeUI binder = IUserNoticeUI.Stub.asInterface(service);
            try {
                binder.setCallbackBinder(mIUserNotice);
            } catch (RemoteException e) {
                Log.w(TAG_USER, "UserNoticeUI Service died", e);
                // Wait for reconnect
                binder = null;
            }
            synchronized (mLock) {
                mUiService = binder;
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            // UI crashed. Stop it so that it does not come again.
            stopUi(/* clearUiShown= */ true);
        }
    };

    // added for debugging purpose
    @GuardedBy("mLock")
    private int mKeyguardPollingCounter;

    private final Runnable mKeyguardPollingRunnable = () -> {
        synchronized (mLock) {
            mKeyguardPollingCounter++;
        }
        startNoticeUiIfNecessary();
    };

    public CarUserNoticeService(Context context) {
        this(context, new Handler(Looper.getMainLooper()));
    }

    @VisibleForTesting
    CarUserNoticeService(Context context, Handler handler) {
        mMainHandler = handler;
        Resources res = context.getResources();
        String componentName = res.getString(R.string.config_userNoticeUiService);
        if (componentName.isEmpty()) {
            // feature disabled
            mContext = null;
            mServiceIntent = null;
            return;
        }
        mContext = context;
        mServiceIntent = new Intent();
        mServiceIntent.setComponent(ComponentName.unflattenFromString(componentName));
    }

    public void ignoreUserNotice(int userId) {
        synchronized (mLock) {
            mIgnoreUserId = userId;
        }
    }

    private boolean checkKeyguardLockedWithPolling() {
        mMainHandler.removeCallbacks(mKeyguardPollingRunnable);
        IWindowManager wm = WindowManagerGlobal.getWindowManagerService();
        boolean locked = true;
        if (wm != null) {
            try {
                locked = wm.isKeyguardLocked();
            } catch (RemoteException e) {
                Log.w(TAG_USER, "system server crashed", e);
            }
        }
        if (locked) {
            mMainHandler.postDelayed(mKeyguardPollingRunnable, KEYGUARD_POLLING_INTERVAL_MS);
        }
        return locked;
    }

    private boolean isNoticeScreenEnabledInSetting(@UserIdInt int userId) {
        return Settings.Secure.getIntForUser(mContext.getContentResolver(),
                CarSettings.Secure.KEY_ENABLE_INITIAL_NOTICE_SCREEN_TO_USER,
                1 /*enable by default*/, userId) == 1;
    }

    private boolean isDisplayOn() {
        PowerManager pm = mContext.getSystemService(PowerManager.class);
        if (pm == null) {
            return false;
        }
        return pm.isInteractive();
    }

    private boolean grantSystemAlertWindowPermission(@UserIdInt int userId) {
        AppOpsManager appOpsManager = mContext.getSystemService(AppOpsManager.class);
        if (appOpsManager == null) {
            Log.w(TAG_USER, "AppOpsManager not ready yet");
            return false;
        }
        String packageName = mServiceIntent.getComponent().getPackageName();
        int packageUid;
        try {
            packageUid = mContext.getPackageManager().getPackageUidAsUser(packageName, userId);
        } catch (PackageManager.NameNotFoundException e) {
            Log.wtf(TAG_USER, "Target package for config_userNoticeUiService not found:"
                    + packageName + " userId:" + userId);
            return false;
        }
        appOpsManager.setMode(AppOpsManager.OP_SYSTEM_ALERT_WINDOW, packageUid, packageName,
                AppOpsManager.MODE_ALLOWED);
        Log.i(TAG_USER, "Granted SYSTEM_ALERT_WINDOW permission to package:" + packageName
                + " package uid:" + packageUid);
        return true;
    }

    private void startNoticeUiIfNecessary() {
        int userId;
        synchronized (mLock) {
            if (mUiShown || mServiceBound) {
                return;
            }
            userId = mUserId;
            if (mIgnoreUserId == userId) {
                return;
            } else {
                mIgnoreUserId = UserHandle.USER_NULL;
            }
        }
        if (userId == UserHandle.USER_NULL) {
            return;
        }
        // headless user 0 is ignored.
        if (userId == UserHandle.USER_SYSTEM) {
            return;
        }
        if (!isNoticeScreenEnabledInSetting(userId)) {
            return;
        }
        if (userId != ActivityManager.getCurrentUser()) {
            // user has switched. will be handled by user switch callback
            return;
        }
        // Dialog can be not shown if display is off.
        // DISPLAY_ON broadcast will handle this later.
        if (!isDisplayOn()) {
            return;
        }
        // Do not show it until keyguard is dismissed.
        if (checkKeyguardLockedWithPolling()) {
            return;
        }
        if (!grantSystemAlertWindowPermission(userId)) {
            return;
        }
        boolean bound = mContext.bindServiceAsUser(mServiceIntent, mUiServiceConnection,
                Context.BIND_AUTO_CREATE, UserHandle.of(userId));
        if (bound) {
            Log.i(TAG_USER, "Bound UserNoticeUI Service Service:" + mServiceIntent);
            synchronized (mLock) {
                mServiceBound = true;
                mUiShown = true;
            }
        } else {
            Log.w(TAG_USER, "Cannot bind to UserNoticeUI Service Service" + mServiceIntent);
        }
    }

    private void stopUi(boolean clearUiShown) {
        mMainHandler.removeCallbacks(mKeyguardPollingRunnable);
        boolean serviceBound;
        synchronized (mLock) {
            mUiService = null;
            serviceBound = mServiceBound;
            mServiceBound = false;
            if (clearUiShown) {
                mUiShown = false;
            }
        }
        if (serviceBound) {
            Log.i(TAG_USER, "Unbound UserNoticeUI Service");
            mContext.unbindService(mUiServiceConnection);
        }
    }

    @Override
    public void init() {
        if (mServiceIntent == null) {
            // feature disabled
            return;
        }

        CarPowerManager carPowerManager;
        synchronized (mLock) {
            mCarPowerManager = CarLocalServices.createCarPowerManager(mContext);
            carPowerManager = mCarPowerManager;
        }
        try {
            carPowerManager.setListener(mPowerStateListener);
        } catch (CarNotConnectedException e) {
            // should not happen
            throw new RuntimeException("CarNotConnectedException from CarPowerManager", e);
        }
        CarUserService userService = CarLocalServices.getService(CarUserService.class);
        userService.addUserLifecycleListener(mUserLifecycleListener);
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_SCREEN_OFF);
        intentFilter.addAction(Intent.ACTION_SCREEN_ON);
        mContext.registerReceiver(mDisplayBroadcastReceiver, intentFilter);
    }

    @Override
    public void release() {
        if (mServiceIntent == null) {
            // feature disabled
            return;
        }
        mContext.unregisterReceiver(mDisplayBroadcastReceiver);
        CarUserService userService = CarLocalServices.getService(CarUserService.class);
        userService.removeUserLifecycleListener(mUserLifecycleListener);
        CarPowerManager carPowerManager;
        synchronized (mLock) {
            carPowerManager = mCarPowerManager;
            mUserId = UserHandle.USER_NULL;
        }
        carPowerManager.clearListener();
        stopUi(/* clearUiShown= */ true);
    }

    @Override
    public void dump(PrintWriter writer) {
        synchronized (mLock) {
            if (mServiceIntent == null) {
                writer.println("*CarUserNoticeService* disabled");
                return;
            }
            if (mUserId == UserHandle.USER_NULL) {
                writer.println("*CarUserNoticeService* User not started yet.");
                return;
            }
            writer.println("*CarUserNoticeService* mServiceIntent:" + mServiceIntent
                    + ", mUserId:" + mUserId
                    + ", mUiShown:" + mUiShown
                    + ", mServiceBound:" + mServiceBound
                    + ", mKeyguardPollingCounter:" + mKeyguardPollingCounter
                    + ", Setting enabled:" + isNoticeScreenEnabledInSetting(mUserId)
                    + ", Ignore User: " + mIgnoreUserId);
        }
    }
}
