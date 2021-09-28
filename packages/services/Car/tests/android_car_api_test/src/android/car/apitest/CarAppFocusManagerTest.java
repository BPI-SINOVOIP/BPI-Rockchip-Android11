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
package android.car.apitest;

import static android.car.CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED;
import static android.car.CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.car.Car;
import android.car.CarAppFocusManager;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;

import androidx.test.filters.FlakyTest;
import androidx.test.filters.RequiresDevice;

import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

@MediumTest
public class CarAppFocusManagerTest extends CarApiTestBase {
    private static final String TAG = CarAppFocusManagerTest.class.getSimpleName();
    private CarAppFocusManager mManager;

    private final LooperThread mEventThread = new LooperThread();

    @Before
    public void setUp() throws Exception {
        mManager = (CarAppFocusManager) getCar().getCarManager(Car.APP_FOCUS_SERVICE);
        assertThat(mManager).isNotNull();

        // Request all application focuses and abandon them to ensure no active context is present
        // when test starts.
        int[] activeTypes =  mManager.getActiveAppTypes();
        FocusOwnershipCallback owner = new FocusOwnershipCallback(/* assertEventThread= */ false);
        for (int i = 0; i < activeTypes.length; i++) {
            mManager.requestAppFocus(activeTypes[i], owner);
            owner.waitForOwnershipGrantAndAssert(DEFAULT_WAIT_TIMEOUT_MS, activeTypes[i]);
            mManager.abandonAppFocus(owner, activeTypes[i]);
            owner.waitForOwnershipLossAndAssert(
                    DEFAULT_WAIT_TIMEOUT_MS, activeTypes[i]);
        }
        mEventThread.start();
        mEventThread.waitForReadyState();
    }

    @Test
    public void testSetActiveNullListener() throws Exception {
        assertThrows(IllegalArgumentException.class,
                () -> mManager.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, null));
    }

    @Test
    public void testRegisterNull() throws Exception {
        assertThrows(IllegalArgumentException.class, () -> mManager.addFocusListener(null, 0));
    }

    @Test
    public void testRegisterUnregister() throws Exception {
        FocusChangedListener listener = new FocusChangedListener();
        FocusChangedListener listener2 = new FocusChangedListener();
        mManager.addFocusListener(listener, 1);
        mManager.addFocusListener(listener2, 1);
        mManager.removeFocusListener(listener);
        mManager.removeFocusListener(listener2);
        mManager.removeFocusListener(listener2);  // Double-unregister is OK
    }

    @Test
    public void testRegisterUnregisterSpecificApp() throws Exception {
        FocusChangedListener listener1 = new FocusChangedListener();
        FocusChangedListener listener2 = new FocusChangedListener();

        CarAppFocusManager manager = createManager();
        manager.addFocusListener(listener1, APP_FOCUS_TYPE_NAVIGATION);
        manager.addFocusListener(listener2, APP_FOCUS_TYPE_NAVIGATION);

        manager.removeFocusListener(listener1, APP_FOCUS_TYPE_NAVIGATION);

        assertThat(manager.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, new FocusOwnershipCallback()))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);

        // Unregistred from nav app, no events expected.
        assertThat(listener1.waitForFocusChangeAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION, true)).isFalse();
        assertThat(listener2.waitForFocusChangeAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();

        manager.removeFocusListener(listener2, APP_FOCUS_TYPE_NAVIGATION);
        assertThat(manager.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, new FocusOwnershipCallback()))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(listener2.waitForFocusChangeAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION, true)).isFalse();

        manager.removeFocusListener(listener2, 2);
        manager.removeFocusListener(listener2, 2);    // Double-unregister is OK
    }

    @Test
    @FlakyTest
    public void testFocusChange() throws Exception {
        CarAppFocusManager manager1 = createManager();
        CarAppFocusManager manager2 = createManager();
        assertThat(manager2).isNotNull();

        assertThat(manager1.getActiveAppTypes()).asList().isEmpty();
        FocusChangedListener change1 = new FocusChangedListener();
        FocusChangedListener change2 = new FocusChangedListener();
        FocusOwnershipCallback owner1 = new FocusOwnershipCallback();
        FocusOwnershipCallback owner2 = new FocusOwnershipCallback();
        manager1.addFocusListener(change1, APP_FOCUS_TYPE_NAVIGATION);
        manager2.addFocusListener(change2, APP_FOCUS_TYPE_NAVIGATION);


        assertThat(manager1.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, owner1))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(owner1.waitForOwnershipGrantAndAssert(
        DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        int expectedFocus  = APP_FOCUS_TYPE_NAVIGATION;
        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(change2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
        APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();
        assertThat(change1.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
        APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();

        expectedFocus = APP_FOCUS_TYPE_NAVIGATION;
        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();

        // this should be no-op
        change1.reset();
        change2.reset();
        assertThat(manager1.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, owner1))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(owner1.waitForOwnershipGrantAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION)).isTrue();

        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(change2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();
        assertThat(change1.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();

        assertThat(manager2.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, owner2))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(owner2.waitForOwnershipGrantAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION)).isTrue();

        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(owner1.waitForOwnershipLossAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION)).isTrue();

        // no-op as it is not owning it
        change1.reset();
        change2.reset();
        manager1.abandonAppFocus(owner1, APP_FOCUS_TYPE_NAVIGATION);
        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();

        change1.reset();
        change2.reset();
        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isTrue();
        expectedFocus = APP_FOCUS_TYPE_NAVIGATION;
        assertThat(manager1.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();
        assertThat(manager2.getActiveAppTypes()).asList().containsExactly(expectedFocus).inOrder();

        change1.reset();
        change2.reset();
        manager2.abandonAppFocus(owner2, APP_FOCUS_TYPE_NAVIGATION);
        assertThat(manager1.isOwningFocus(owner1, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager2.isOwningFocus(owner2, APP_FOCUS_TYPE_NAVIGATION)).isFalse();
        assertThat(manager1.getActiveAppTypes()).asList().isEmpty();
        assertThat(manager2.getActiveAppTypes()).asList().isEmpty();
        assertThat(change1.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, false)).isTrue();

        manager1.removeFocusListener(change1);
        manager2.removeFocusListener(change2);
    }

    @RequiresDevice
    @Test
    public void testFilter() throws Exception {
        CarAppFocusManager manager1 = createManager(getContext(), mEventThread);
        CarAppFocusManager manager2 = createManager(getContext(), mEventThread);

        assertThat(manager1.getActiveAppTypes()).asList().isEmpty();
        assertThat(manager2.getActiveAppTypes()).asList().isEmpty();

        FocusChangedListener listener1 = new FocusChangedListener();
        FocusChangedListener listener2 = new FocusChangedListener();
        FocusOwnershipCallback owner = new FocusOwnershipCallback();
        manager1.addFocusListener(listener1, APP_FOCUS_TYPE_NAVIGATION);
        manager2.addFocusListener(listener2, APP_FOCUS_TYPE_NAVIGATION);

        assertThat(manager1.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, owner))
                .isEqualTo(APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(owner.waitForOwnershipGrantAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION)).isTrue();

        assertThat(listener1.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();
        assertThat(listener2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();

        listener1.reset();
        listener2.reset();
        manager1.abandonAppFocus(owner, APP_FOCUS_TYPE_NAVIGATION);
        assertThat(listener1.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
        APP_FOCUS_TYPE_NAVIGATION, false)).isTrue();
        assertThat(listener2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
        APP_FOCUS_TYPE_NAVIGATION, false)).isTrue();
    }

    private CarAppFocusManager createManager() throws InterruptedException {
        return createManager(getContext(), mEventThread);
    }

    private static CarAppFocusManager createManager(Context context,
            LooperThread eventThread) throws InterruptedException {
        Car car = createCar(context, eventThread);
        CarAppFocusManager manager = (CarAppFocusManager) car.getCarManager(Car.APP_FOCUS_SERVICE);
        assertThat(manager).isNotNull();
        return manager;
    }

    private static Car createCar(Context context, LooperThread eventThread)
            throws InterruptedException {
        DefaultServiceConnectionListener connectionListener =
                new DefaultServiceConnectionListener();
        Car car = Car.createCar(context, connectionListener, eventThread.mHandler);
        assertThat(car).isNotNull();
        car.connect();
        connectionListener.waitForConnection(DEFAULT_WAIT_TIMEOUT_MS);
        return car;
    }

    @RequiresDevice
    @Test
    public void testMultipleChangeListenersPerManager() throws Exception {
        CarAppFocusManager manager = createManager();
        FocusChangedListener listener = new FocusChangedListener();
        FocusChangedListener listener2 = new FocusChangedListener();
        FocusOwnershipCallback owner = new FocusOwnershipCallback();
        manager.addFocusListener(listener, APP_FOCUS_TYPE_NAVIGATION);
        manager.addFocusListener(listener2, APP_FOCUS_TYPE_NAVIGATION);

        assertThat(manager.requestAppFocus(APP_FOCUS_TYPE_NAVIGATION, owner))
                .isEqualTo(APP_FOCUS_REQUEST_SUCCEEDED);
        assertThat(owner.waitForOwnershipGrantAndAssert(
                DEFAULT_WAIT_TIMEOUT_MS, APP_FOCUS_TYPE_NAVIGATION)).isTrue();

        assertThat(listener.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();
        assertThat(listener2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, true)).isTrue();

        listener.reset();
        listener2.reset();
        manager.abandonAppFocus(owner, APP_FOCUS_TYPE_NAVIGATION);
        assertThat(listener.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, false)).isTrue();
        assertThat(listener2.waitForFocusChangeAndAssert(DEFAULT_WAIT_TIMEOUT_MS,
                APP_FOCUS_TYPE_NAVIGATION, false)).isTrue();
    }

    private final class FocusChangedListener
            implements CarAppFocusManager.OnAppFocusChangedListener {
        private volatile int mLastChangeAppType;
        private volatile boolean mLastChangeAppActive;
        private volatile Semaphore mChangeWait = new Semaphore(0);

        boolean waitForFocusChangeAndAssert(long timeoutMs, int expectedAppType,
                boolean expectedAppActive) throws Exception {

            if (!mChangeWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS)) {
                return false;
            }

            assertThat(mLastChangeAppType).isEqualTo(expectedAppType);
            assertThat(mLastChangeAppActive).isEqualTo(expectedAppActive);
            return true;
        }

        void reset() throws InterruptedException {
            mLastChangeAppType = 0;
            mLastChangeAppActive = false;
            mChangeWait.drainPermits();
        }

        @Override
        public void onAppFocusChanged(int appType, boolean active) {
            assertEventThread();
            mLastChangeAppType = appType;
            mLastChangeAppActive = active;
            mChangeWait.release();
        }
    }

    private final class FocusOwnershipCallback
            implements CarAppFocusManager.OnAppFocusOwnershipCallback {
        private int mLastLossEvent;
        private final Semaphore mLossEventWait = new Semaphore(0);
        private int mLastGrantEvent;
        private final Semaphore mGrantEventWait = new Semaphore(0);
        private final boolean mAssertEventThread;

        private FocusOwnershipCallback(boolean assertEventThread) {
            mAssertEventThread = assertEventThread;
        }

        private FocusOwnershipCallback() {
            this(true);
        }

        boolean waitForOwnershipLossAndAssert(long timeoutMs, int expectedAppType)
                throws Exception {
            if (!mLossEventWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS)) {
                return false;
            }
            assertThat(mLastLossEvent).isEqualTo(expectedAppType);
            return true;
        }

        boolean waitForOwnershipGrantAndAssert(long timeoutMs, int expectedAppType)
                throws Exception {
            if (!mGrantEventWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS)) {
                return false;
            }
            assertThat(mLastGrantEvent).isEqualTo(expectedAppType);
            return true;
        }

        @Override
        public void onAppFocusOwnershipLost(int appType) {
            Log.i(TAG, "onAppFocusOwnershipLost " + appType);
            if (mAssertEventThread) {
                assertEventThread();
            }
            mLastLossEvent = appType;
            mLossEventWait.release();
        }

        @Override
        public void onAppFocusOwnershipGranted(int appType) {
            Log.i(TAG, "onAppFocusOwnershipGranted " + appType);
            mLastGrantEvent = appType;
            mGrantEventWait.release();
        }
    }

    private void assertEventThread() {
        assertThat(Thread.currentThread()).isSameAs(mEventThread);
    }

    private static final class LooperThread extends Thread {

        private final Object mReadySync = new Object();

        volatile Handler mHandler;

        @Override
        public void run() {
            Looper.prepare();
            mHandler = new Handler();

            synchronized (mReadySync) {
                mReadySync.notifyAll();
            }

            Looper.loop();
        }

        void waitForReadyState() throws InterruptedException {
            synchronized (mReadySync) {
                mReadySync.wait(DEFAULT_WAIT_TIMEOUT_MS);
            }
        }
    }
}
