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

import static android.car.test.mocks.AndroidMockitoHelper.mockQueryService;
import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetAllUsers;
import static android.car.test.mocks.AndroidMockitoHelper.mockUmIsUserRunning;
import static android.car.test.util.UserTestingHelper.UserInfoBuilder;
import static android.car.watchdog.CarWatchdogManager.TIMEOUT_CRITICAL;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.automotive.watchdog.ICarWatchdog;
import android.automotive.watchdog.ICarWatchdogClient;
import android.car.Car;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.watchdog.CarWatchdogManager;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * <p>This class contains unit tests for the {@link CarWatchdogService}.
 */
@RunWith(MockitoJUnitRunner.class)
public class CarWatchdogServiceTest extends AbstractExtendedMockitoTestCase {

    private static final String CAR_WATCHDOG_DAEMON_INTERFACE =
            "android.automotive.watchdog.ICarWatchdog/default";
    private static final int MAX_WAIT_TIME_MS = 3000;
    private static final int INVALID_SESSION_ID = -1;

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private final Executor mExecutor =
            InstrumentationRegistry.getInstrumentation().getTargetContext().getMainExecutor();
    private final ArrayList<UserInfo> mUserInfos = new ArrayList<>(Arrays.asList(
            new UserInfoBuilder(10).setName("user 1").build(),
            new UserInfoBuilder(11).setName("user 2").build()
    ));

    @Mock private Context mMockContext;
    @Mock private Car mCar;
    @Mock private UserManager mUserManager;
    @Mock private IBinder mDaemonBinder;
    @Mock private IBinder mServiceBinder;
    @Mock private ICarWatchdog mCarWatchdogDaemon;

    private CarWatchdogService mCarWatchdogService;
    private ICarWatchdogClient mWatchdogServiceClientImpl;

    @Before
    public void setUpMocks() throws Exception {
        mCarWatchdogService = new CarWatchdogService(mMockContext);

        mockQueryService(CAR_WATCHDOG_DAEMON_INTERFACE, mDaemonBinder, mCarWatchdogDaemon);
        when(mCar.getEventHandler()).thenReturn(mMainHandler);
        when(mServiceBinder.queryLocalInterface(anyString())).thenReturn(mCarWatchdogService);
        when(mMockContext.getSystemService(Context.USER_SERVICE)).thenReturn(mUserManager);
        mockUmGetAllUsers(mUserManager, mUserInfos);
        mockUmIsUserRunning(mUserManager, 10, true);
        mockUmIsUserRunning(mUserManager, 11, false);

        mCarWatchdogService.init();
        mWatchdogServiceClientImpl = registerMediator();
    }

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder builder) {
        builder
            .spyStatic(ServiceManager.class)
            .spyStatic(UserHandle.class);
    }

    @Test
    public void testRegisterUnregisterClient() throws Exception {
        TestClient client = new TestClient(new SelfCheckGoodClient());
        client.registerClient();
        assertThat(mCarWatchdogService.getClientCount(TIMEOUT_CRITICAL)).isEqualTo(1);
        client.unregisterClient();
        assertThat(mCarWatchdogService.getClientCount(TIMEOUT_CRITICAL)).isEqualTo(0);
    }

    @Test
    public void testNoSelfCheckGoodClient() throws Exception {
        testClientResponse(new NoSelfCheckGoodClient(), 0);
    }

    @Test
    public void testSelfCheckGoodClient() throws Exception {
        testClientResponse(new SelfCheckGoodClient(), 0);
    }

    @Test
    public void testBadClient() throws Exception {
        BadTestClient client = new BadTestClient();
        testClientResponse(client, 1);
        assertThat(client.makeSureProcessTerminationNotified()).isEqualTo(true);
    }

    @Test
    public void testClientUnderStoppedUser() throws Exception {
        expectStoppedUser();
        TestClient client = new TestClient(new BadTestClient());
        client.registerClient();
        mWatchdogServiceClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        ArgumentCaptor<int[]> notRespondingClients = ArgumentCaptor.forClass(int[].class);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), notRespondingClients.capture(), eq(123456));
        assertThat(notRespondingClients.getValue().length).isEqualTo(0);
        mWatchdogServiceClientImpl.checkIfAlive(987654, TIMEOUT_CRITICAL);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), notRespondingClients.capture(), eq(987654));
        assertThat(notRespondingClients.getValue().length).isEqualTo(0);
    }

    @Test
    public void testMultipleClients() throws Exception {
        expectRunningUser();
        ArgumentCaptor<int[]> pidsCaptor = ArgumentCaptor.forClass(int[].class);
        ArrayList<TestClient> clients = new ArrayList<>(Arrays.asList(
                new TestClient(new NoSelfCheckGoodClient()),
                new TestClient(new SelfCheckGoodClient()),
                new TestClient(new BadTestClient()),
                new TestClient(new BadTestClient())
        ));
        for (int i = 0; i < clients.size(); i++) {
            clients.get(i).registerClient();
        }

        mWatchdogServiceClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        for (int i = 0; i < clients.size(); i++) {
            assertThat(clients.get(i).mAndroidClient.makeSureHealthCheckDone()).isEqualTo(true);
        }
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), pidsCaptor.capture(), eq(123456));
        assertThat(pidsCaptor.getValue().length).isEqualTo(0);

        mWatchdogServiceClientImpl.checkIfAlive(987654, TIMEOUT_CRITICAL);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), pidsCaptor.capture(), eq(987654));
        assertThat(pidsCaptor.getValue().length).isEqualTo(2);
    }

    private ICarWatchdogClient registerMediator() throws Exception {
        ArgumentCaptor<ICarWatchdogClient> clientImplCaptor =
                ArgumentCaptor.forClass(ICarWatchdogClient.class);
        verify(mCarWatchdogDaemon).registerMediator(clientImplCaptor.capture());
        return clientImplCaptor.getValue();
    }

    private void testClientResponse(BaseAndroidClient androidClient, int badClientCount)
            throws Exception {
        expectRunningUser();
        TestClient client = new TestClient(androidClient);
        client.registerClient();
        mWatchdogServiceClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        ArgumentCaptor<int[]> notRespondingClients = ArgumentCaptor.forClass(int[].class);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), notRespondingClients.capture(), eq(123456));
        // Checking Android client health is asynchronous, so wait at most 1 second.
        int repeat = 10;
        while (repeat > 0) {
            int sessionId = androidClient.getLastSessionId();
            if (sessionId != INVALID_SESSION_ID) {
                break;
            }
            SystemClock.sleep(100L);
            repeat--;
        }
        assertThat(androidClient.getLastSessionId()).isNotEqualTo(INVALID_SESSION_ID);
        assertThat(notRespondingClients.getValue().length).isEqualTo(0);
        assertThat(androidClient.makeSureHealthCheckDone()).isEqualTo(true);
        mWatchdogServiceClientImpl.checkIfAlive(987654, TIMEOUT_CRITICAL);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(
                eq(mWatchdogServiceClientImpl), notRespondingClients.capture(), eq(987654));
        assertThat(notRespondingClients.getValue().length).isEqualTo(badClientCount);
    }

    private void expectRunningUser() {
        doReturn(10).when(() -> UserHandle.getUserId(Binder.getCallingUid()));
    }

    private void expectStoppedUser() {
        doReturn(11).when(() -> UserHandle.getUserId(Binder.getCallingUid()));
    }

    private final class TestClient {
        final CarWatchdogManager mCarWatchdogManager;
        BaseAndroidClient mAndroidClient;

        TestClient(BaseAndroidClient actualClient) {
            mCarWatchdogManager = new CarWatchdogManager(mCar, mServiceBinder);
            mAndroidClient = actualClient;
            actualClient.setManager(mCarWatchdogManager);
        }

        public void registerClient() {
            mCarWatchdogManager.registerClient(mExecutor, mAndroidClient, TIMEOUT_CRITICAL);
        }

        public void unregisterClient() {
            mCarWatchdogManager.unregisterClient(mAndroidClient);
        }
    }

    private abstract class BaseAndroidClient extends CarWatchdogManager.CarWatchdogClientCallback {
        protected final CountDownLatch mLatchHealthCheckDone = new CountDownLatch(1);
        protected final CountDownLatch mLatchProcessTermination = new CountDownLatch(1);
        protected CarWatchdogManager mManager;
        protected int mLastSessionId = INVALID_SESSION_ID;

        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            mLastSessionId = sessionId;
            return false;
        }

        @Override
        public void onPrepareProcessTermination() {
            mLatchProcessTermination.countDown();
        }

        public int getLastSessionId() {
            return mLastSessionId;
        }

        public boolean makeSureProcessTerminationNotified() {
            try {
                return mLatchProcessTermination.await(1000, TimeUnit.MILLISECONDS);
            } catch (InterruptedException ignore) {
            }
            return false;
        }

        public boolean makeSureHealthCheckDone() {
            try {
                return mLatchHealthCheckDone.await(1000, TimeUnit.MILLISECONDS);
            } catch (InterruptedException ignore) {
            }
            return false;
        }

        public void setManager(CarWatchdogManager manager) {
            mManager = manager;
        }
    }

    private final class SelfCheckGoodClient extends BaseAndroidClient {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            super.onCheckHealthStatus(sessionId, timeout);
            mMainHandler.post(() -> {
                mManager.tellClientAlive(this, sessionId);
                mLatchHealthCheckDone.countDown();
            });
            return false;
        }
    }

    private final class NoSelfCheckGoodClient extends BaseAndroidClient {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            super.onCheckHealthStatus(sessionId, timeout);
            mLatchHealthCheckDone.countDown();
            return true;
        }
    }

    private final class BadTestClient extends BaseAndroidClient {
        @Override
        public boolean onCheckHealthStatus(int sessionId, int timeout) {
            super.onCheckHealthStatus(sessionId, timeout);
            mLatchHealthCheckDone.countDown();
            return false;
        }
    }
}
