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

package android.net.ipsec.ike;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.pm.PackageManager;
import android.os.Looper;
import android.os.test.TestLooper;
import android.util.Log;

import com.android.internal.net.ipsec.ike.IkeSessionStateMachine;
import com.android.internal.net.ipsec.ike.IkeSessionStateMachineTest;
import com.android.internal.net.ipsec.ike.IkeSessionTestBase;

import org.junit.Before;
import org.junit.Test;

import java.net.Inet4Address;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

public final class IkeSessionTest extends IkeSessionTestBase {
    private static final int TIMEOUT_MS = 500;

    private IkeSessionParams mIkeSessionParams;
    private ChildSessionParams mMockChildSessionParams;
    private Executor mUserCbExecutor;
    private IkeSessionCallback mMockIkeSessionCb;
    private ChildSessionCallback mMockChildSessionCb;

    @Before
    public void setUp() throws Exception {
        super.setUp();

        if (Looper.myLooper() == null) Looper.prepare();

        mIkeSessionParams = buildIkeSessionParams();
        mMockChildSessionParams = mock(ChildSessionParams.class);
        mUserCbExecutor = (r) -> r.run(); // Inline executor for testing purposes.
        mMockIkeSessionCb = mock(IkeSessionCallback.class);
        mMockChildSessionCb = mock(ChildSessionCallback.class);
    }


    private IkeSessionParams buildIkeSessionParams() throws Exception {
        return new IkeSessionParams.Builder(mMockConnectManager)
                .setServerHostname(REMOTE_ADDRESS.getHostAddress())
                .addSaProposal(IkeSessionStateMachineTest.buildSaProposal())
                .setLocalIdentification(new IkeIpv4AddrIdentification((Inet4Address) LOCAL_ADDRESS))
                .setRemoteIdentification(
                        new IkeIpv4AddrIdentification((Inet4Address) REMOTE_ADDRESS))
                .setAuthPsk(new byte[0] /* psk, unused */)
                .build();
    }

    @Test
    public void testConstructIkeSession() throws Exception {
        IkeSession ikeSession =
                new IkeSession(
                        mSpyContext,
                        mIpSecManager,
                        mIkeSessionParams,
                        mMockChildSessionParams,
                        mUserCbExecutor,
                        mMockIkeSessionCb,
                        mMockChildSessionCb);
        assertNotNull(ikeSession.mIkeSessionStateMachine.getHandler().getLooper());
        ikeSession.kill();
    }

    /**
     * Test that when users construct IkeSessions from different threads, these IkeSessions will
     * still be running on the same IKE worker thread.
     */
    @Test
    public void testConstructFromDifferentThreads() throws Exception {
        final int numSession = 2;
        IkeSession[] sessions = new IkeSession[numSession];

        final CountDownLatch cntLatch = new CountDownLatch(2);

        for (int i = 0; i < numSession; i++) {
            int index = i;
            new Thread() {
                @Override
                public void run() {
                    try {
                        sessions[index] =
                                new IkeSession(
                                        mSpyContext,
                                        mIpSecManager,
                                        mIkeSessionParams,
                                        mMockChildSessionParams,
                                        mUserCbExecutor,
                                        mMockIkeSessionCb,
                                        mMockChildSessionCb);
                        cntLatch.countDown();
                    } catch (Exception e) {
                        Log.e("IkeSessionTest", "error encountered constructing IkeSession. ", e);
                    }
                }
            }.start();
        }

        assertTrue(cntLatch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Verify that two sessions use the same looper.
        assertEquals(
                sessions[0].mIkeSessionStateMachine.getHandler().getLooper(),
                sessions[1].mIkeSessionStateMachine.getHandler().getLooper());

        for (IkeSession s : sessions) {
            s.kill();
        }
    }

    @Test
    public void testOpensIkeSession() throws Exception {
        TestLooper testLooper = new TestLooper();
        IkeSession ikeSession =
                new IkeSession(
                        testLooper.getLooper(),
                        mSpyContext,
                        mIpSecManager,
                        mIkeSessionParams,
                        mMockChildSessionParams,
                        mUserCbExecutor,
                        mMockIkeSessionCb,
                        mMockChildSessionCb);
        testLooper.dispatchAll();

        assertTrue(
                ikeSession.mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeInit);

        ikeSession.kill();
        testLooper.dispatchAll();
    }

    @Test
    public void testThrowWhenSetupTunnelWithMissingFeature() {
        PackageManager mockPackageMgr = mock(PackageManager.class);
        doReturn(mockPackageMgr).when(mSpyContext).getPackageManager();
        doReturn(false).when(mockPackageMgr).hasSystemFeature(PackageManager.FEATURE_IPSEC_TUNNELS);

        try {
            IkeSession ikeSession =
                    new IkeSession(
                            mSpyContext,
                            mIpSecManager,
                            mIkeSessionParams,
                            mock(TunnelModeChildSessionParams.class),
                            mUserCbExecutor,
                            mMockIkeSessionCb,
                            mMockChildSessionCb);
            fail("Expected to fail due to missing FEATURE_IPSEC_TUNNELS");
        } catch (Exception expected) {

        }
    }
}
