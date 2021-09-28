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

package android.car.watchdoglib;

import static android.car.test.mocks.AndroidMockitoHelper.mockQueryService;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.testng.Assert.assertThrows;

import android.automotive.watchdog.ICarWatchdog;
import android.automotive.watchdog.ICarWatchdogClient;
import android.automotive.watchdog.ICarWatchdogMonitor;
import android.automotive.watchdog.PowerCycle;
import android.automotive.watchdog.StateType;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoSession;
import org.mockito.Spy;
import org.mockito.quality.Strictness;

import java.util.ArrayList;

/**
 * <p>This class contains unit tests for the {@link CarWatchdogDaemonHelper}.
 */
public class CarWatchdogDaemonHelperTest {

    private static final String CAR_WATCHDOG_DAEMON_INTERFACE =
            "android.automotive.watchdog.ICarWatchdog/default";

    @Mock CarWatchdogDaemonHelper.OnConnectionChangeListener mListener;
    @Mock private IBinder mBinder = new Binder();
    @Spy private ICarWatchdog mFakeCarWatchdog = new FakeCarWatchdog();
    private CarWatchdogDaemonHelper mCarWatchdogDaemonHelper;
    private MockitoSession mMockSession;

    @Before
    public void setUp() {
        mMockSession = mockitoSession()
                .initMocks(this)
                .strictness(Strictness.LENIENT)
                .spyStatic(ServiceManager.class)
                .startMocking();
        mockQueryService(CAR_WATCHDOG_DAEMON_INTERFACE, mBinder, mFakeCarWatchdog);
        mCarWatchdogDaemonHelper = new CarWatchdogDaemonHelper();
        mCarWatchdogDaemonHelper.connect();
    }

    @After
    public void tearDown() {
        mMockSession.finishMocking();
    }

    @Test
    public void testConnection() {
        CarWatchdogDaemonHelper carWatchdogDaemonHelper = new CarWatchdogDaemonHelper();
        carWatchdogDaemonHelper.addOnConnectionChangeListener(mListener);
        carWatchdogDaemonHelper.connect();
        verify(mListener).onConnectionChange(true);
    }

    @Test
    public void testRemoveConnectionChangeListener() {
        CarWatchdogDaemonHelper carWatchdogDaemonHelper = new CarWatchdogDaemonHelper();
        carWatchdogDaemonHelper.addOnConnectionChangeListener(mListener);
        carWatchdogDaemonHelper.removeOnConnectionChangeListener(mListener);
        carWatchdogDaemonHelper.connect();
        verify(mListener, never()).onConnectionChange(true);
    }

    @Test
    public void testIndirectCall_RegisterUnregisterClient() throws Exception {
        ICarWatchdogClient client = new ICarWatchdogClient.Default();
        mCarWatchdogDaemonHelper.registerClient(client, 0);
        verify(mFakeCarWatchdog).registerClient(client, 0);
        mCarWatchdogDaemonHelper.unregisterClient(client);
        verify(mFakeCarWatchdog).unregisterClient(client);
    }

    @Test
    public void testIndirectCall_RegisterUnregisterMediator() throws Exception {
        ICarWatchdogClient mediator = new ICarWatchdogClient.Default();
        mCarWatchdogDaemonHelper.registerMediator(mediator);
        verify(mFakeCarWatchdog).registerMediator(mediator);
        mCarWatchdogDaemonHelper.unregisterMediator(mediator);
        verify(mFakeCarWatchdog).unregisterMediator(mediator);
    }

    @Test
    public void testIndirectCall_RegisterUnregisterMonitor() throws Exception {
        ICarWatchdogMonitor monitor = new ICarWatchdogMonitor.Default();
        mCarWatchdogDaemonHelper.registerMonitor(monitor);
        verify(mFakeCarWatchdog).registerMonitor(monitor);
        mCarWatchdogDaemonHelper.unregisterMonitor(monitor);
        verify(mFakeCarWatchdog).unregisterMonitor(monitor);
    }

    @Test
    public void testIndirectCall_TellClientAlive() throws Exception {
        ICarWatchdogClient client = new ICarWatchdogClient.Default();
        mCarWatchdogDaemonHelper.tellClientAlive(client, 123456);
        verify(mFakeCarWatchdog).tellClientAlive(client, 123456);
    }

    @Test
    public void testIndirectCall_TellMediatorAlive() throws Exception {
        ICarWatchdogClient mediator = new ICarWatchdogClient.Default();
        int[] pids = new int[]{111};
        mCarWatchdogDaemonHelper.tellMediatorAlive(mediator, pids, 123456);
        verify(mFakeCarWatchdog).tellMediatorAlive(mediator, pids, 123456);
    }

    @Test
    public void testIndirectCall_TellDumpFinished() throws Exception {
        ICarWatchdogMonitor monitor = new ICarWatchdogMonitor.Default();
        mCarWatchdogDaemonHelper.tellDumpFinished(monitor, 123456);
        verify(mFakeCarWatchdog).tellDumpFinished(monitor, 123456);
    }

    @Test
    public void testIndirectCall_NotifySystemStateChange() throws Exception {
        mCarWatchdogDaemonHelper.notifySystemStateChange(StateType.POWER_CYCLE,
                PowerCycle.POWER_CYCLE_SUSPEND, -1);
        verify(mFakeCarWatchdog).notifySystemStateChange(StateType.POWER_CYCLE,
                PowerCycle.POWER_CYCLE_SUSPEND, -1);
    }

    /*
     * Test that the {@link CarWatchdogDaemonHelper} throws {@code IllegalArgumentException} when
     * trying to register already-registered client again.
     */
    @Test
    public void testMultipleRegistration() throws Exception {
        ICarWatchdogClient client = new ICarWatchdogClientImpl();
        mCarWatchdogDaemonHelper.registerMediator(client);
        assertThrows(IllegalArgumentException.class,
                () -> mCarWatchdogDaemonHelper.registerMediator(client));
    }

    /*
     * Test that the {@link CarWatchdogDaemonHelper} throws {@code IllegalArgumentException} when
     * trying to unregister not-registered client.
     */
    @Test
    public void testInvalidUnregistration() throws Exception {
        ICarWatchdogClient client = new ICarWatchdogClientImpl();
        assertThrows(IllegalArgumentException.class,
                () -> mCarWatchdogDaemonHelper.unregisterMediator(client));
    }

    // FakeCarWatchdog mimics ICarWatchdog daemon in local process.
    private final class FakeCarWatchdog extends ICarWatchdog.Default {

        private final ArrayList<ICarWatchdogClient> mClients = new ArrayList<>();

        @Override
        public void registerMediator(ICarWatchdogClient mediator) throws RemoteException {
            for (ICarWatchdogClient client : mClients) {
                if (client == mediator) {
                    throw new IllegalArgumentException("Already registered mediator");
                }
            }
            mClients.add(mediator);
        }

        @Override
        public void unregisterMediator(ICarWatchdogClient mediator) throws RemoteException {
            for (ICarWatchdogClient client : mClients) {
                if (client == mediator) {
                    mClients.remove(mediator);
                    return;
                }
            }
            throw new IllegalArgumentException("Not registered mediator");
        }

    }

    private final class ICarWatchdogClientImpl extends ICarWatchdogClient.Stub {
        @Override
        public void checkIfAlive(int sessionId, int timeout) {}

        @Override
        public void prepareProcessTermination() {}

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }
}
