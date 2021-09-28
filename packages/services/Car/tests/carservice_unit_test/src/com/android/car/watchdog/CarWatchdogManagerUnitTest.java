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

import static android.car.watchdog.CarWatchdogManager.TIMEOUT_CRITICAL;

import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.car.Car;
import android.car.watchdog.CarWatchdogManager;
import android.car.watchdog.ICarWatchdogServiceCallback;
import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.concurrent.Executor;

@RunWith(MockitoJUnitRunner.class)
public class CarWatchdogManagerUnitTest {

    private static final int MAX_WAIT_TIME_MS = 3000;

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private final Context mContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private final Executor mExecutor = mContext.getMainExecutor();

    @Mock private Car mCar;
    @Mock private IBinder mBinder;
    @Mock private CarWatchdogService mService;

    private CarWatchdogManager mCarWatchdogManager;

    @Before
    public void setUp() {
        when(mCar.getEventHandler()).thenReturn(mMainHandler);
        when(mBinder.queryLocalInterface(anyString())).thenReturn(mService);
        mCarWatchdogManager = new CarWatchdogManager(mCar, mBinder);
    }

    @Test
    public void testRegisterClient() throws Exception {
        TestClient client = new TestClient();
        ICarWatchdogServiceCallback clientImpl = registerClient(client);
        mCarWatchdogManager.unregisterClient(client);
        verify(mService).unregisterClient(clientImpl);

        clientImpl.onCheckHealthStatus(123456, TIMEOUT_CRITICAL);
        verify(mService, never()).tellClientAlive(clientImpl, 123456);
    }

    @Test
    public void testUnregisterUnregisteredClient() throws Exception {
        TestClient client = new TestClient();
        mCarWatchdogManager.registerClient(mExecutor, client, TIMEOUT_CRITICAL);
        mCarWatchdogManager.unregisterClient(client);
        // The following call should not throw an exception.
        mCarWatchdogManager.unregisterClient(client);
    }

    @Test
    public void testRegisterMultipleClients() {
        TestClient client1 = new TestClient();
        TestClient client2 = new TestClient();
        mCarWatchdogManager.registerClient(mExecutor, client1, TIMEOUT_CRITICAL);
        assertThrows(IllegalStateException.class,
                () -> mCarWatchdogManager.registerClient(mExecutor, client2, TIMEOUT_CRITICAL));
    }

    @Test
    public void testHandlePongOnlyClient() throws Exception {
        testClientResponse(new TestClient());
    }

    @Test
    public void testHandleRedundantPongClient() throws Exception {
        testClientResponse(new RedundantPongClient());
    }

    @Test
    public void testHandleReturnAndPongClient() throws Exception {
        testClientResponse(new ReturnAndPongClient());
    }

    private ICarWatchdogServiceCallback registerClient(
            CarWatchdogManager.CarWatchdogClientCallback client) {
        mCarWatchdogManager.registerClient(mExecutor, client, TIMEOUT_CRITICAL);
        ArgumentCaptor<ICarWatchdogServiceCallback> clientImplCaptor =
                ArgumentCaptor.forClass(ICarWatchdogServiceCallback.class);

        verify(mService).registerClient(clientImplCaptor.capture(), eq(TIMEOUT_CRITICAL));
        return clientImplCaptor.getValue();
    }

    private void testClientResponse(CarWatchdogManager.CarWatchdogClientCallback client)
            throws Exception {
        ICarWatchdogServiceCallback clientImpl = registerClient(client);
        clientImpl.onCheckHealthStatus(123456, TIMEOUT_CRITICAL);
        verify(mService, timeout(MAX_WAIT_TIME_MS)).tellClientAlive(clientImpl, 123456);
    }

    private final class TestClient extends CarWatchdogManager.CarWatchdogClientCallback {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            mMainHandler.post(() -> mCarWatchdogManager.tellClientAlive(this, sessionId));
            return false;
        }
    }

    private final class RedundantPongClient extends CarWatchdogManager.CarWatchdogClientCallback {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            mCarWatchdogManager.tellClientAlive(this, sessionId);
            mCarWatchdogManager.tellClientAlive(this, sessionId);
            return false;
        }
    }

    private final class ReturnAndPongClient extends CarWatchdogManager.CarWatchdogClientCallback {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            mMainHandler.post(() -> mCarWatchdogManager.tellClientAlive(this, sessionId));
            return true;
        }
    }
}
