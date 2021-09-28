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

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.app.Service;
import android.car.Car;
import android.car.CarProjectionManager;
import android.car.CarProjectionManager.ProjectionAccessPointCallback;
import android.content.Intent;
import android.net.wifi.SoftApConfiguration;
import android.os.Binder;
import android.os.IBinder;
import android.test.suitebuilder.annotation.LargeTest;

import androidx.test.filters.RequiresDevice;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@LargeTest
public class CarProjectionManagerTest extends CarApiTestBase {
    private static final String TAG = CarProjectionManagerTest.class.getSimpleName();

    private final CarProjectionManager.CarProjectionListener mListener = (fromLongPress) -> { };

    private CarProjectionManager mManager;

    public static final class TestService extends Service {
        public static Object mLock = new Object();
        private static boolean sBound;
        private final Binder mBinder = new Binder() {};

        private static synchronized void setBound(boolean bound) {
            sBound = bound;
        }

        public static synchronized boolean getBound() {
            return sBound;
        }

        @Override
        public IBinder onBind(Intent intent) {
            setBound(true);
            synchronized (mLock) {
                mLock.notify();
            }
            return mBinder;
        }
    }

    @Before
    public void setUp() throws Exception {
        mManager = (CarProjectionManager) getCar().getCarManager(Car.PROJECTION_SERVICE);
        assertThat(mManager).isNotNull();
    }

    @Test
    public void testSetUnsetListeners() throws Exception {
        mManager.registerProjectionListener(
                mListener, CarProjectionManager.PROJECTION_VOICE_SEARCH);
        mManager.unregisterProjectionListener();
    }

    @Test
    public void testRegisterListenersHandleBadInput() throws Exception {
        assertThrows(NullPointerException.class, () -> mManager.registerProjectionListener(null,
                CarProjectionManager.PROJECTION_VOICE_SEARCH));
    }

    @Test
    public void testRegisterProjectionRunner() throws Exception {
        Intent intent = new Intent(
                InstrumentationRegistry.getInstrumentation().getContext(), TestService.class);
        assertThat(TestService.getBound()).isFalse();
        mManager.registerProjectionRunner(intent);
        synchronized (TestService.mLock) {
            try {
                TestService.mLock.wait(1000);
            } catch (InterruptedException e) {
                // Do nothing
            }
        }
        assertThat(TestService.getBound()).isTrue();
        mManager.unregisterProjectionRunner(intent);
    }


    @Ignore("//TODO(b/120081013): move this test to CTS")
    @RequiresDevice
    @Test
    public void testAccessPoint() throws Exception {
        CountDownLatch startedLatch = new CountDownLatch(1);

        mManager.startProjectionAccessPoint(new ProjectionAccessPointCallback() {
            @Override
            public void onStarted(SoftApConfiguration softApConfiguration) {
                startedLatch.countDown();
            }
        });

        assertThat(startedLatch.await(30, TimeUnit.SECONDS)).isTrue();

        mManager.stopProjectionAccessPoint();
    }
}
