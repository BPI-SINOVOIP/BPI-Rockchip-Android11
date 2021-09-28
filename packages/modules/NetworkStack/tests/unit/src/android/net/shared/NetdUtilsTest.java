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

package android.net.shared;

import static android.net.INetd.LOCAL_NET_ID;
import static android.system.OsConstants.EBUSY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.net.INetd;
import android.net.IpPrefix;
import android.os.RemoteException;
import android.os.ServiceSpecificException;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
@SmallTest
public final class NetdUtilsTest {
    private static final String IFACE_NAME = "testnet1";
    private static final IpPrefix TEST_IPPREFIX = new IpPrefix("192.168.42.1/24");

    @Mock private INetd mNetd;

    @Before public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    private void setNetworkAddInterfaceOutcome(final Exception cause, int numLoops)
            throws Exception {
        // This cannot be an int because local variables referenced from a lambda expression must
        // be final or effectively final.
        final Counter myCounter = new Counter();
        doAnswer((invocation) -> {
            myCounter.count();
            if (myCounter.isCounterReached(numLoops)) {
                if (cause == null) return null;

                throw cause;
            }

            throw new ServiceSpecificException(EBUSY);
        }).when(mNetd).networkAddInterface(LOCAL_NET_ID, IFACE_NAME);
    }

    class Counter {
        private int mValue = 0;

        private void count() {
            mValue++;
        }

        private boolean isCounterReached(int target) {
            return mValue >= target;
        }
    }

    private void verifyTetherInterfaceSucceeds(int expectedTries) throws Exception {
        setNetworkAddInterfaceOutcome(null, expectedTries);

        NetdUtils.tetherInterface(mNetd, IFACE_NAME, TEST_IPPREFIX);
        verify(mNetd).tetherInterfaceAdd(IFACE_NAME);
        verify(mNetd, times(expectedTries)).networkAddInterface(LOCAL_NET_ID, IFACE_NAME);
        verify(mNetd, times(2)).networkAddRoute(eq(LOCAL_NET_ID), eq(IFACE_NAME), any(), any());
        verifyNoMoreInteractions(mNetd);
        reset(mNetd);
    }

    @Test
    public void testTetherInterfaceSuccessful() throws Exception {
        // Expect #networkAddInterface successful at first tries.
        verifyTetherInterfaceSucceeds(1);

        // Expect #networkAddInterface successful after 10 tries.
        verifyTetherInterfaceSucceeds(10);
    }

    private void runTetherInterfaceWithServiceSpecificException(int expectedTries,
            int expectedCode) throws Exception {
        setNetworkAddInterfaceOutcome(new ServiceSpecificException(expectedCode), expectedTries);

        try {
            NetdUtils.tetherInterface(mNetd, IFACE_NAME, TEST_IPPREFIX, 20, 0);
            fail("Expect throw ServiceSpecificException");
        } catch (ServiceSpecificException e) {
            assertEquals(e.errorCode, expectedCode);
        }

        verifyNetworkAddInterfaceFails(expectedTries);
        reset(mNetd);
    }

    private void runTetherInterfaceWithRemoteException(int expectedTries) throws Exception {
        setNetworkAddInterfaceOutcome(new RemoteException(), expectedTries);

        try {
            NetdUtils.tetherInterface(mNetd, IFACE_NAME, TEST_IPPREFIX, 20, 0);
            fail("Expect throw RemoteException");
        } catch (RemoteException e) { }

        verifyNetworkAddInterfaceFails(expectedTries);
        reset(mNetd);
    }

    private void verifyNetworkAddInterfaceFails(int expectedTries) throws Exception {
        verify(mNetd).tetherInterfaceAdd(IFACE_NAME);
        verify(mNetd, times(expectedTries)).networkAddInterface(LOCAL_NET_ID, IFACE_NAME);
        verify(mNetd, never()).networkAddRoute(anyInt(), anyString(), any(), any());
        verifyNoMoreInteractions(mNetd);
    }

    @Test
    public void testTetherInterfaceFailOnNetworkAddInterface() throws Exception {
        // Test throwing ServiceSpecificException with EBUSY failure.
        runTetherInterfaceWithServiceSpecificException(20, EBUSY);

        // Test throwing ServiceSpecificException with unexpectedError.
        final int unexpectedError = 999;
        runTetherInterfaceWithServiceSpecificException(1, unexpectedError);

        // Test throwing ServiceSpecificException with unexpectedError after 7 tries.
        runTetherInterfaceWithServiceSpecificException(7, unexpectedError);

        // Test throwing RemoteException.
        runTetherInterfaceWithRemoteException(1);

        // Test throwing RemoteException after 3 tries.
        runTetherInterfaceWithRemoteException(3);
    }
}
