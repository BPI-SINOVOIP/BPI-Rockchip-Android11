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

package com.android.car;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.doReturn;

import android.car.Car;
import android.car.CarAppFocusManager;
import android.car.IAppFocusListener;
import android.car.IAppFocusOwnershipCallback;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

@RunWith(MockitoJUnitRunner.class)
public class AppFocusServiceTest {

    private static final long WAIT_TIMEOUT_MS = 500;

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    @Mock
    private Context mContext;
    @Mock
    private SystemActivityMonitoringService mSystemActivityMonitoringService;
    @Mock
    private Car mCar;

    private AppFocusService mService;
    private CarAppFocusManager mCarAppFocusManager1;
    private CarAppFocusManager mCarAppFocusManager2;

    private AppFocusChangedListener mAppFocusChangedListener1 = new AppFocusChangedListener();

    private AppFocusOwnershipCallback mAppFocusOwnershipCallback1 = new AppFocusOwnershipCallback();

    @Before
    public void setUp() {
        mService = new AppFocusService(mContext, mSystemActivityMonitoringService);
        mService.init();
        doReturn(mMainHandler).when(mCar).getEventHandler();
        mCarAppFocusManager1 = new CarAppFocusManager(mCar, mService.asBinder());
        mCarAppFocusManager2 = new CarAppFocusManager(mCar, mService.asBinder());
    }

    @Test
    public void testSingleOwner() throws Exception {
        mCarAppFocusManager2.addFocusListener(mAppFocusChangedListener1,
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);

        int r = mCarAppFocusManager1.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mAppFocusOwnershipCallback1);
        assertThat(r).isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(mCarAppFocusManager1.isOwningFocus(mAppFocusOwnershipCallback1,
                CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED)).isTrue();
        waitForNavFocusChangeAndAssert(mAppFocusChangedListener1, true);

        mAppFocusChangedListener1.resetWait();
        mCarAppFocusManager1.abandonAppFocus(mAppFocusOwnershipCallback1,
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        assertThat(mCarAppFocusManager1.isOwningFocus(mAppFocusOwnershipCallback1,
                CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED)).isFalse();
        waitForNavFocusChangeAndAssert(mAppFocusChangedListener1, false);
    }

    private void waitForNavFocusChangeAndAssert(AppFocusChangedListener listener, boolean isActive)
            throws Exception {
        listener.waitForEvent();
        if (isActive) {
            assertThat(listener.mLastActive).isTrue();
        } else {
            assertThat(listener.mLastActive).isFalse();
        }
        assertThat(listener.mLastAppType).isEqualTo(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
    }

    @Test
    public void testOwnerBinderDeath() throws Exception {
        mCarAppFocusManager2.addFocusListener(mAppFocusChangedListener1,
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);

        int r = mCarAppFocusManager1.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mAppFocusOwnershipCallback1);
        assertThat(r).isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(mCarAppFocusManager1.isOwningFocus(mAppFocusOwnershipCallback1,
                CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED)).isTrue();
        waitForNavFocusChangeAndAssert(mAppFocusChangedListener1, true);

        assertThat(mService.mAllOwnershipClients.getInterfaces()).hasSize(1);
        BinderInterfaceContainer.BinderInterface<IAppFocusOwnershipCallback> binder =
                mService.mAllOwnershipClients.getInterfaces().iterator().next();
        // Now fake binder death
        mAppFocusChangedListener1.resetWait();
        binder.binderDied();
        assertThat(mService.mAllOwnershipClients.getInterfaces()).isEmpty();
        waitForNavFocusChangeAndAssert(mAppFocusChangedListener1, false);
    }

    @Test
    public void testListenerBinderDeath() throws Exception {

        mCarAppFocusManager1.addFocusListener(mAppFocusChangedListener1,
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        assertThat(mService.mAllChangeClients.getInterfaces()).hasSize(1);
        BinderInterfaceContainer.BinderInterface<IAppFocusListener> binder =
                mService.mAllChangeClients.getInterfaces().iterator().next();
        binder.binderDied();
        assertThat(mService.mAllChangeClients.getInterfaces()).isEmpty();
    }

    private class AppFocusChangedListener implements CarAppFocusManager.OnAppFocusChangedListener {

        private final Semaphore mSemaphore = new Semaphore(0);
        private int mLastAppType;
        private boolean mLastActive;

        @Override
        public void onAppFocusChanged(int appType, boolean active) {
            mLastAppType = appType;
            mLastActive = active;
            mSemaphore.release();
        }

        public void waitForEvent() throws Exception {
            assertThat(mSemaphore.tryAcquire(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS)).isTrue();
        }

        public void resetWait() {
            mSemaphore.drainPermits();
        }
    }

    private class AppFocusOwnershipCallback implements
            CarAppFocusManager.OnAppFocusOwnershipCallback {

        private final Semaphore mSemaphore = new Semaphore(0);
        private int mGrantedAppTypes;

        @Override
        public void onAppFocusOwnershipLost(int appType) {
            mGrantedAppTypes = mGrantedAppTypes & ~appType;
            mSemaphore.release();
        }

        @Override
        public void onAppFocusOwnershipGranted(int appType) {
            mGrantedAppTypes = mGrantedAppTypes | appType;
            mSemaphore.release();
        }

        public void waitForEvent() throws Exception {
            mSemaphore.tryAcquire(WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public void resetWait() {
            mSemaphore.drainPermits();
        }
    }
}
