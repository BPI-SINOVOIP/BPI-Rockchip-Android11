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

import static android.car.test.mocks.AndroidMockitoHelper.mockUmGetUsers;
import static android.car.watchdog.CarWatchdogManager.TIMEOUT_CRITICAL;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.automotive.watchdog.ICarWatchdog;
import android.automotive.watchdog.ICarWatchdogClient;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.watchdog.ICarWatchdogServiceCallback;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.IBinder;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;

/**
 * <p>This class contains unit tests for the {@link CarWatchdogService}.
 */
@RunWith(MockitoJUnitRunner.class)
public class CarWatchdogServiceUnitTest extends AbstractExtendedMockitoTestCase {

    private static final String CAR_WATCHDOG_DAEMON_INTERFACE =
            "android.automotive.watchdog.ICarWatchdog/default";
    private static final int MAX_WAIT_TIME_MS = 3000;
    private static final int INVALID_SESSION_ID = -1;

    @Mock private Context mMockContext;
    @Mock private UserManager mUserManager;
    @Mock private IBinder mBinder;
    @Mock private ICarWatchdog mCarWatchdogDaemon;

    private CarWatchdogService mCarWatchdogService;
    private ICarWatchdogClient mClientImpl;

    /**
     * Initialize all of the objects with the @Mock annotation.
     */
    @Before
    public void setUpMocks() throws Exception {
        mCarWatchdogService = new CarWatchdogService(mMockContext);
        mockWatchdogDaemon();
        setupUsers();
        mCarWatchdogService.init();
        mClientImpl = registerMediator();
    }

    @Test
    public void testMediatorHealthCheck() throws Exception {
        mClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(eq(mClientImpl),
                any(int[].class), eq(123456));
    }

    @Test
    public void testRegisterClient() throws Exception {
        TestClient client = new TestClient();
        mCarWatchdogService.registerClient(client, TIMEOUT_CRITICAL);
        mClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        // Checking client health is asynchronous, so wait at most 1 second.
        int repeat = 10;
        while (repeat > 0) {
            int sessionId = client.getLastSessionId();
            if (sessionId != INVALID_SESSION_ID) {
                return;
            }
            SystemClock.sleep(100L);
            repeat--;
        }
        assertThat(client.getLastSessionId()).isNotEqualTo(INVALID_SESSION_ID);
    }

    @Test
    public void testUnregisterUnregisteredClient() throws Exception {
        TestClient client = new TestClient();
        mCarWatchdogService.registerClient(client, TIMEOUT_CRITICAL);
        mCarWatchdogService.unregisterClient(client);
        mClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        assertThat(client.getLastSessionId()).isEqualTo(INVALID_SESSION_ID);
    }

    @Test
    public void testGoodClientHealthCheck() throws Exception {
        testClientHealthCheck(new TestClient(), 0);
    }

    @Test
    public void testBadClientHealthCheck() throws Exception {
        testClientHealthCheck(new BadTestClient(), 1);
    }

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder builder) {
        builder.spyStatic(ServiceManager.class);
    }

    private void mockWatchdogDaemon() {
        doReturn(mBinder).when(() -> ServiceManager.getService(CAR_WATCHDOG_DAEMON_INTERFACE));
        when(mBinder.queryLocalInterface(anyString())).thenReturn(mCarWatchdogDaemon);
    }

    private void setupUsers() {
        when(mMockContext.getSystemService(Context.USER_SERVICE)).thenReturn(mUserManager);
        mockUmGetUsers(mUserManager, new ArrayList<UserInfo>());
    }

    private ICarWatchdogClient registerMediator() throws Exception {
        ArgumentCaptor<ICarWatchdogClient> clientImplCaptor =
                ArgumentCaptor.forClass(ICarWatchdogClient.class);
        verify(mCarWatchdogDaemon).registerMediator(clientImplCaptor.capture());
        return clientImplCaptor.getValue();
    }

    private void testClientHealthCheck(TestClient client, int badClientCount) throws Exception {
        mCarWatchdogService.registerClient(client, TIMEOUT_CRITICAL);
        mClientImpl.checkIfAlive(123456, TIMEOUT_CRITICAL);
        ArgumentCaptor<int[]> notRespondingClients = ArgumentCaptor.forClass(int[].class);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(eq(mClientImpl),
                notRespondingClients.capture(), eq(123456));
        assertThat(notRespondingClients.getValue().length).isEqualTo(0);
        mClientImpl.checkIfAlive(987654, TIMEOUT_CRITICAL);
        verify(mCarWatchdogDaemon, timeout(MAX_WAIT_TIME_MS)).tellMediatorAlive(eq(mClientImpl),
                notRespondingClients.capture(), eq(987654));
        assertThat(notRespondingClients.getValue().length).isEqualTo(badClientCount);
    }

    private class TestClient extends ICarWatchdogServiceCallback.Stub {
        protected int mLastSessionId = INVALID_SESSION_ID;

        @Override
        public void onCheckHealthStatus(int sessionId, int timeout) {
            mLastSessionId = sessionId;
            mCarWatchdogService.tellClientAlive(this, sessionId);
        }

        @Override
        public void onPrepareProcessTermination() {}

        public int getLastSessionId() {
            return mLastSessionId;
        }
    }

    private final class BadTestClient extends TestClient {
        @Override
        public void onCheckHealthStatus(int sessionId, int timeout) {
            mLastSessionId = sessionId;
            // This client doesn't respond to CarWatchdogService.
        }
    }
}
