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
package android.car.cts;

import static com.google.common.truth.Truth.assertThat;

import android.car.Car;
import android.content.ComponentName;
import android.content.Context;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.RequiredFeatureRule;

import org.junit.After;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class CarTest {
    @ClassRule
    public static final RequiredFeatureRule sRequiredFeatureRule = new RequiredFeatureRule(
            PackageManager.FEATURE_AUTOMOTIVE);

    private static final long DEFAULT_WAIT_TIMEOUT_MS = 2000;

    private Context mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();

    private Car mCar;
    private DefaultServiceConnectionListener mServiceConnectionListener;

    @After
    public void tearDown() {
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
        }
    }

    @Test
    public void testConnection() throws Exception {
        mServiceConnectionListener = new DefaultServiceConnectionListener();
        mCar = Car.createCar(mContext, mServiceConnectionListener);
        assertThat(mCar.isConnected()).isFalse();
        assertThat(mCar.isConnecting()).isFalse();
        mCar.connect();
        mServiceConnectionListener.waitForConnection(DEFAULT_WAIT_TIMEOUT_MS);
        assertThat(mServiceConnectionListener.isConnected()).isTrue();
        assertThat(mCar.getCarConnectionType()).isEqualTo(Car.CONNECTION_TYPE_EMBEDDED);
        mCar.disconnect();
        assertThat(mCar.isConnected()).isFalse();
        assertThat(mCar.isConnecting()).isFalse();
    }

    @Test
    public void testBlockingCreateCar() throws Exception {
        mCar = Car.createCar(mContext);
        assertConnectedCar(mCar);
    }

    @Test
    public void testConnectionType() throws Exception {
        createCarAndRunOnReady((car) -> assertThat(car.getCarConnectionType()).isEqualTo(
                Car.CONNECTION_TYPE_EMBEDDED));
    }

    @Test
    public void testIsFeatureEnabled() throws Exception {
        createCarAndRunOnReady(
                (car) -> assertThat(car.isFeatureEnabled(Car.AUDIO_SERVICE)).isTrue());
    }

    @Test
    public void testCreateCarWaitForever() throws Exception {
        CarServiceLifecycleListenerImpl listenerImpl = new CarServiceLifecycleListenerImpl(null);
        mCar = Car.createCar(mContext, /* handler= */ null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                listenerImpl);
        assertConnectedCar(mCar);
        listenerImpl.waitForReady(DEFAULT_WAIT_TIMEOUT_MS);
    }

    @Test
    public void testCreateCarNoWait() throws Exception {
        CarServiceLifecycleListenerImpl listenerImpl = new CarServiceLifecycleListenerImpl(null);
        // car service should be already running, so for normal apps, this should always return
        // immediately
        mCar = Car.createCar(mContext, /* handler= */ null, Car.CAR_WAIT_TIMEOUT_DO_NOT_WAIT,
                listenerImpl);
        assertConnectedCar(mCar);
        listenerImpl.waitForReady(DEFAULT_WAIT_TIMEOUT_MS);
    }

    private static void assertConnectedCar(Car car) {
        assertThat(car).isNotNull();
        assertThat(car.isConnected()).isTrue();
    }

    private interface ReadyListener {
        void onReady(Car car);
    }

    private void createCarAndRunOnReady(ReadyListener readyListener) throws Exception {
        CarServiceLifecycleListenerImpl listenerImpl = new CarServiceLifecycleListenerImpl(
                readyListener);
        mCar = Car.createCar(mContext, /* handler= */ null, DEFAULT_WAIT_TIMEOUT_MS,
                listenerImpl);
        assertConnectedCar(mCar);
        listenerImpl.waitForReady(DEFAULT_WAIT_TIMEOUT_MS);
    }

    private static final class CarServiceLifecycleListenerImpl
            implements Car.CarServiceLifecycleListener {

        private final ReadyListener mReadyListener;
        private final CountDownLatch mWaitLatch = new CountDownLatch(1);

        private CarServiceLifecycleListenerImpl(@Nullable ReadyListener readyListener) {
            mReadyListener = readyListener;
        }

        private void waitForReady(long waitTimeMs) throws Exception {
            assertThat(mWaitLatch.await(waitTimeMs, TimeUnit.MILLISECONDS)).isTrue();
        }

        @Override
        public void onLifecycleChanged(@NonNull Car car, boolean ready) {
            assertConnectedCar(car);
            assertThat(ready).isTrue();
            if (mReadyListener != null) {
                mReadyListener.onReady(car);
            }
            mWaitLatch.countDown();
        }
    }

    protected class DefaultServiceConnectionListener implements ServiceConnection {
        private final Semaphore mConnectionWait = new Semaphore(0);

        private boolean mIsconnected = false;

        public synchronized boolean isConnected() {
            return mIsconnected;
        }

        public void waitForConnection(long timeoutMs) throws InterruptedException {
            mConnectionWait.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            synchronized (this) {
                mIsconnected = false;
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            synchronized (this) {
                mIsconnected = true;
            }
            mConnectionWait.release();
        }
    }
}
