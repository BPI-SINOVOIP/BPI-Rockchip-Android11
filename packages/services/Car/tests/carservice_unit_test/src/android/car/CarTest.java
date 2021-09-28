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

package android.car;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doAnswer;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.Nullable;
import android.content.ComponentName;
import android.content.Context;
import android.os.IBinder;
import android.os.Looper;
import android.os.ServiceManager;
import android.util.Pair;

import com.android.car.CarServiceUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoSession;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.quality.Strictness;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Unit test for Car API.
 */
@RunWith(JUnit4.class)
public class CarTest {
    private static final String TAG = CarTest.class.getSimpleName();

    private MockitoSession mMockingSession;

    @Mock
    private Context mContext;

    private int mGetServiceCallCount;

    // It is tricky to mock this. So create dummy version instead.
    private ICar.Stub mService = new ICar.Stub() {
        @Override
        public void setCarServiceHelper(android.os.IBinder helper) {
        }

        @Override
        public void onUserLifecycleEvent(int eventType, long timestampMs, int fromUserId,
                int toUserId) {
        }

        @Override
        public void onFirstUserUnlocked(int userId, long timestampMs, long duration,
                int halResponseTime) {
        }

        @Override
        public void getInitialUserInfo(int requestType, int timeoutMs, IBinder binder) {
        }

        @Override
        public void setInitialUser(int userId) {
        }

        @Override
        public boolean isFeatureEnabled(String featureName) {
            return false;
        }

        @Override
        public int enableFeature(String featureName) {
            return Car.FEATURE_REQUEST_SUCCESS;
        }

        @Override
        public int disableFeature(String featureName) {
            return Car.FEATURE_REQUEST_SUCCESS;
        }

        @Override
        public List<String> getAllEnabledFeatures() {
            return Collections.EMPTY_LIST;
        }

        @Override
        public List<String> getAllPendingDisabledFeatures() {
            return Collections.EMPTY_LIST;
        }

        @Override
        public List<String> getAllPendingEnabledFeatures() {
            return Collections.EMPTY_LIST;
        }

        @Override
        public String getCarManagerClassForFeature(String featureName) {
            return null;
        }

        @Override
        public android.os.IBinder getCarService(java.lang.String serviceName) {
            return null;
        }

        @Override
        public int getCarConnectionType() {
            return 0;
        }
    };

    private class LifecycleListener implements Car.CarServiceLifecycleListener {
        // Use thread safe one to prevent adding another lock for testing
        private CopyOnWriteArrayList<Pair<Car, Boolean>> mEvents = new CopyOnWriteArrayList<>();

        @Override
        public void onLifecycleChanged(Car car, boolean ready) {
            assertThat(Looper.getMainLooper()).isEqualTo(Looper.myLooper());
            mEvents.add(new Pair<>(car, ready));
        }
    }

    private  final LifecycleListener mLifecycleListener = new LifecycleListener();

    @Before
    public void setUp() {
        mMockingSession = mockitoSession()
                .initMocks(this)
                .mockStatic(ServiceManager.class)
                .strictness(Strictness.LENIENT)
                .startMocking();
        mGetServiceCallCount = 0;
    }

    @After
    public void tearDown() {
        mMockingSession.finishMocking();
    }

    private void expectService(@Nullable IBinder service) {
        doReturn(service).when(
                () -> ServiceManager.getService(Car.CAR_SERVICE_BINDER_SERVICE_NAME));
    }

    private void expectBindService() {
        when(mContext.bindServiceAsUser(anyObject(), anyObject(), anyInt(),
                anyObject())).thenReturn(true);
    }

    private void returnServiceAfterNSereviceManagerCalls(int returnNonNullAfterThisCall) {
        doAnswer((InvocationOnMock invocation)  -> {
            mGetServiceCallCount++;
            if (mGetServiceCallCount > returnNonNullAfterThisCall) {
                return mService;
            } else {
                return null;
            }
        }).when(() -> ServiceManager.getService(Car.CAR_SERVICE_BINDER_SERVICE_NAME));
    }

    private void assertServiceBoundOnce() {
        verify(mContext, times(1)).bindServiceAsUser(anyObject(), anyObject(), anyInt(),
                anyObject());
    }

    private void assertOneListenerCallAndClear(Car expectedCar, boolean ready) {
        assertThat(mLifecycleListener.mEvents).containsExactly(new Pair<>(expectedCar, ready));
        mLifecycleListener.mEvents.clear();
    }

    @Test
    public void testCreateCarSuccessWithCarServiceRunning() {
        expectService(mService);
        Car car = Car.createCar(mContext);
        assertThat(car).isNotNull();
        car.disconnect();
    }

    @Test
    public void testCreateCarReturnNull() {
        // car service is not running yet and bindService does not bring the service yet.
        // createCar should timeout and give up.
        expectService(null);
        assertThat(Car.createCar(mContext)).isNull();
    }

    @Test
    public void testCreateCarOkWhenCarServiceIsStarted() {
        returnServiceAfterNSereviceManagerCalls(10);
        // Car service is not running yet and binsService call should start it.
        expectBindService();
        Car car = Car.createCar(mContext);
        assertThat(car).isNotNull();
        assertServiceBoundOnce();
        car.disconnect();
    }

    @Test
    public void testCreateCarWithStatusChangeNoServiceConnectionWithCarServiceStarted() {
        returnServiceAfterNSereviceManagerCalls(10);
        expectBindService();
        Car car = Car.createCar(mContext, null,
                Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, mLifecycleListener);
        assertThat(car).isNotNull();
        assertServiceBoundOnce();
        waitForMainToBeComplete();
        assertOneListenerCallAndClear(car, true);

        // Just call these to guarantee that nothing crashes with these call.
        runOnMainSyncSafe(() -> {
            car.getServiceConnectionListener().onServiceConnected(new ComponentName("", ""),
                    mService);
            car.getServiceConnectionListener().onServiceDisconnected(new ComponentName("", ""));
        });
    }

    @Test
    public void testCreateCarWithStatusChangeNoServiceHandleCarServiceRestart() {
        expectService(mService);
        expectBindService();
        Car car = Car.createCar(mContext, null,
                Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, mLifecycleListener);
        assertThat(car).isNotNull();
        assertServiceBoundOnce();

        // fake connection
        runOnMainSyncSafe(() ->
                car.getServiceConnectionListener().onServiceConnected(new ComponentName("", ""),
                        mService));
        waitForMainToBeComplete();
        assertOneListenerCallAndClear(car, true);

        // fake crash
        runOnMainSyncSafe(() ->
                car.getServiceConnectionListener().onServiceDisconnected(
                        new ComponentName("", "")));
        waitForMainToBeComplete();
        assertOneListenerCallAndClear(car, false);


        // fake restart
        runOnMainSyncSafe(() ->
                car.getServiceConnectionListener().onServiceConnected(new ComponentName("", ""),
                        mService));
        waitForMainToBeComplete();
        assertOneListenerCallAndClear(car, true);
    }

    @Test
    public void testCreateCarWithStatusChangeDirectCallInsideMainForServiceAlreadyReady() {
        expectService(mService);
        expectBindService();
        runOnMainSyncSafe(() -> {
            Car car = Car.createCar(mContext, null,
                    Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, mLifecycleListener);
            assertThat(car).isNotNull();
            verify(mContext, times(1)).bindServiceAsUser(anyObject(), anyObject(), anyInt(),
                    anyObject());
            // mLifecycleListener should have been called as this is main thread.
            assertOneListenerCallAndClear(car, true);
        });
    }

    @Test
    public void testCreateCarWithStatusChangeDirectCallInsideMainForServiceReadyLater() {
        returnServiceAfterNSereviceManagerCalls(10);
        expectBindService();
        runOnMainSyncSafe(() -> {
            Car car = Car.createCar(mContext, null,
                    Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, mLifecycleListener);
            assertThat(car).isNotNull();
            assertServiceBoundOnce();
            assertOneListenerCallAndClear(car, true);
        });
    }

    private void runOnMainSyncSafe(Runnable runnable) {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            runnable.run();
        } else {
            CarServiceUtils.runOnMainSync(runnable);
        }
    }
    private void waitForMainToBeComplete() {
        // dispatch dummy runnable and confirm that it is done.
        runOnMainSyncSafe(() -> { });
    }
}
