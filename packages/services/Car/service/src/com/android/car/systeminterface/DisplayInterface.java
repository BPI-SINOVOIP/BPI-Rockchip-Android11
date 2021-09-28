/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.car.systeminterface;

import static com.android.settingslib.display.BrightnessUtils.GAMMA_SPACE_MAX;
import static com.android.settingslib.display.BrightnessUtils.convertGammaToLinear;
import static com.android.settingslib.display.BrightnessUtils.convertLinearToGamma;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.Settings.SettingNotFoundException;
import android.provider.Settings.System;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;

import com.android.car.CarLog;
import com.android.car.CarPowerManagementService;
import com.android.internal.annotations.GuardedBy;

/**
 * Interface that abstracts display operations
 */
public interface DisplayInterface {
    /**
     * @param brightness Level from 0 to 100%
     */
    void setDisplayBrightness(int brightness);
    void setDisplayState(boolean on);
    void startDisplayStateMonitoring(CarPowerManagementService service);
    void stopDisplayStateMonitoring();

    /**
     * Refreshing display brightness. Used when user is switching and car turned on.
     */
    void refreshDisplayBrightness();

    /**
     * Default implementation of display operations
     */
    class DefaultImpl implements DisplayInterface {
        private final ActivityManager mActivityManager;
        private final ContentResolver mContentResolver;
        private final Context mContext;
        private final DisplayManager mDisplayManager;
        private final InputManager mInputManager;
        private final Object mLock = new Object();
        private final int mMaximumBacklight;
        private final int mMinimumBacklight;
        private final PowerManager mPowerManager;
        private final WakeLockInterface mWakeLockInterface;
        @GuardedBy("mLock")
        private CarPowerManagementService mService;
        @GuardedBy("mLock")
        private boolean mDisplayStateSet;
        @GuardedBy("mLock")
        private int mLastBrightnessLevel = -1;

        private final ContentObserver mBrightnessObserver =
                new ContentObserver(new Handler(Looper.getMainLooper())) {
                    @Override
                    public void onChange(boolean selfChange) {
                        refreshDisplayBrightness();
                    }
                };

        private final DisplayManager.DisplayListener mDisplayListener = new DisplayListener() {
            @Override
            public void onDisplayAdded(int displayId) {
                //ignore
            }

            @Override
            public void onDisplayRemoved(int displayId) {
                //ignore
            }

            @Override
            public void onDisplayChanged(int displayId) {
                if (displayId == Display.DEFAULT_DISPLAY) {
                    handleMainDisplayChanged();
                }
            }
        };

        private final BroadcastReceiver mUserChangeReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                onUsersUpdate();
            }
        };

        DefaultImpl(Context context, WakeLockInterface wakeLockInterface) {
            mActivityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            mContext = context;
            mContentResolver = mContext.getContentResolver();
            mDisplayManager = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
            mInputManager = (InputManager) mContext.getSystemService(Context.INPUT_SERVICE);
            mPowerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            mMaximumBacklight = mPowerManager.getMaximumScreenBrightnessSetting();
            mMinimumBacklight = mPowerManager.getMinimumScreenBrightnessSetting();
            mWakeLockInterface = wakeLockInterface;

            mContext.registerReceiverAsUser(
                    mUserChangeReceiver,
                    UserHandle.ALL,
                    new IntentFilter(Intent.ACTION_USER_SWITCHED),
                    null,
                    null);
        }

        @Override
        public void refreshDisplayBrightness() {
            synchronized (mLock) {
                if (mService == null) {
                    Log.e(CarLog.TAG_POWER,
                            "Could not set brightness: no CarPowerManagementService");
                    return;
                }
                int gamma = GAMMA_SPACE_MAX;
                try {
                    int linear = System.getIntForUser(
                            mContentResolver,
                            System.SCREEN_BRIGHTNESS,
                            ActivityManager.getCurrentUser());
                    gamma = convertLinearToGamma(linear, mMinimumBacklight, mMaximumBacklight);
                } catch (SettingNotFoundException e) {
                    Log.e(CarLog.TAG_POWER, "Could not get SCREEN_BRIGHTNESS: " + e);
                }
                int percentBright = (gamma * 100 + ((GAMMA_SPACE_MAX + 1) / 2)) / GAMMA_SPACE_MAX;
                mService.sendDisplayBrightness(percentBright);
            }
        }

        private void handleMainDisplayChanged() {
            boolean isOn = isMainDisplayOn();
            CarPowerManagementService service;
            synchronized (mLock) {
                if (mDisplayStateSet == isOn) { // same as what is set
                    return;
                }
                service = mService;
            }
            service.handleMainDisplayChanged(isOn);
        }

        private boolean isMainDisplayOn() {
            Display disp = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
            return disp.getState() == Display.STATE_ON;
        }

        @Override
        public void setDisplayBrightness(int percentBright) {
            synchronized (mLock) {
                if (percentBright == mLastBrightnessLevel) {
                    // We have already set the value last time. Skipping
                    return;
                }
                mLastBrightnessLevel = percentBright;
            }
            int gamma = (percentBright * GAMMA_SPACE_MAX + 50) / 100;
            int linear = convertGammaToLinear(gamma, mMinimumBacklight, mMaximumBacklight);
            System.putIntForUser(
                    mContentResolver,
                    System.SCREEN_BRIGHTNESS,
                    linear,
                    ActivityManager.getCurrentUser());
        }

        @Override
        public void startDisplayStateMonitoring(CarPowerManagementService service) {
            synchronized (mLock) {
                mService = service;
                mDisplayStateSet = isMainDisplayOn();
            }
            mContentResolver.registerContentObserver(
                    System.getUriFor(System.SCREEN_BRIGHTNESS),
                    false,
                    mBrightnessObserver,
                    UserHandle.USER_ALL);
            mDisplayManager.registerDisplayListener(mDisplayListener, service.getHandler());
            refreshDisplayBrightness();
        }

        @Override
        public void stopDisplayStateMonitoring() {
            mDisplayManager.unregisterDisplayListener(mDisplayListener);
            mContentResolver.unregisterContentObserver(mBrightnessObserver);
        }

        @Override
        public void setDisplayState(boolean on) {
            synchronized (mLock) {
                mDisplayStateSet = on;
            }
            if (on) {
                mWakeLockInterface.switchToFullWakeLock();
                Log.i(CarLog.TAG_POWER, "on display");
                mPowerManager.wakeUp(SystemClock.uptimeMillis());
            } else {
                mWakeLockInterface.switchToPartialWakeLock();
                Log.i(CarLog.TAG_POWER, "off display");
                mPowerManager.goToSleep(SystemClock.uptimeMillis());
            }
            // Turn touchscreen input devices on or off, the same as the display
            for (int deviceId : mInputManager.getInputDeviceIds()) {
                InputDevice inputDevice = mInputManager.getInputDevice(deviceId);
                if (inputDevice != null
                        && (inputDevice.getSources() & InputDevice.SOURCE_TOUCHSCREEN)
                        == InputDevice.SOURCE_TOUCHSCREEN) {
                    if (on) {
                        mInputManager.enableInputDevice(deviceId);
                    } else {
                        mInputManager.disableInputDevice(deviceId);
                    }
                }
            }
        }

        private void onUsersUpdate() {
            synchronized (mLock) {
                if (mService == null) {
                    // CarPowerManagementService is not connected yet
                    return;
                }
                // We need to reset last value
                mLastBrightnessLevel = -1;
            }
            refreshDisplayBrightness();
        }
    }
}
