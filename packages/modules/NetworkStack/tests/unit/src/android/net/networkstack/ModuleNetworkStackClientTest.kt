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

package android.net.networkstack

import android.content.Context
import android.net.IIpMemoryStoreCallbacks
import android.net.INetworkMonitorCallbacks
import android.net.INetworkStackConnector
import android.net.Network
import android.net.NetworkStack
import android.net.dhcp.DhcpServingParamsParcel
import android.net.dhcp.IDhcpServerCallbacks
import android.net.ip.IIpClientCallbacks
import android.os.Build
import android.os.IBinder
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.SmallTest
import com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn
import com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession
import com.android.testutils.DevSdkIgnoreRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mock
import org.mockito.Mockito.never
import org.mockito.Mockito.timeout
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
@SmallTest
class ModuleNetworkStackClientTest {
    private val TEST_IFNAME = "testiface"
    private val TEST_NETWORK = Network(43)
    private val TEST_TIMEOUT_MS = 500L

    // ModuleNetworkStackClient is only available after Q
    @Rule @JvmField
    val mIgnoreRule = DevSdkIgnoreRule(ignoreClassUpTo = Build.VERSION_CODES.Q)

    @Mock
    private lateinit var mContext: Context
    @Mock
    private lateinit var mConnectorBinder: IBinder
    @Mock
    private lateinit var mConnector: INetworkStackConnector
    @Mock
    private lateinit var mIpClientCb: IIpClientCallbacks
    @Mock
    private lateinit var mDhcpServerCb: IDhcpServerCallbacks
    @Mock
    private lateinit var mNetworkMonitorCb: INetworkMonitorCallbacks
    @Mock
    private lateinit var mIpMemoryStoreCb: IIpMemoryStoreCallbacks

    @Before
    fun setUp() {
        MockitoAnnotations.initMocks(this)
        doReturn(mConnector).`when`(mConnectorBinder).queryLocalInterface(
                INetworkStackConnector::class.qualifiedName!!)
    }

    @After
    fun tearDown() {
        ModuleNetworkStackClient.resetInstanceForTest()
        NetworkStack.setServiceForTest(null)
    }

    @Test
    fun testIpClientServiceAvailableImmediately() {
        NetworkStack.setServiceForTest(mConnectorBinder)
        ModuleNetworkStackClient.getInstance(mContext).makeIpClient(TEST_IFNAME, mIpClientCb)
        verify(mConnector).makeIpClient(TEST_IFNAME, mIpClientCb)
    }

    @Test
    fun testIpClientServiceAvailableAfterPolling() {
        // Force NetworkStack.getService() to return null: this cannot be done with
        // setServiceForTest, as passing null just restores default behavior.
        val session = mockitoSession().spyStatic(NetworkStack::class.java).startMocking()
        try {
            doReturn(null).`when` { NetworkStack.getService() }
            ModuleNetworkStackClient.getInstance(mContext).makeIpClient(TEST_IFNAME, mIpClientCb)

            Thread.sleep(TEST_TIMEOUT_MS)
            verify(mConnector, never()).makeIpClient(any(), any())
            NetworkStack.setServiceForTest(mConnectorBinder)
        } finally {
            // Restore behavior of NetworkStack to return what was set in setServiceForTest
            session.finishMocking()
        }

        // Use a longer timeout as polling can cause larger delays
        verify(mConnector, timeout(TEST_TIMEOUT_MS * 4)).makeIpClient(TEST_IFNAME, mIpClientCb)
    }

    @Test
    fun testDhcpServerAvailableImmediately() {
        NetworkStack.setServiceForTest(mConnectorBinder)
        val testParams = DhcpServingParamsParcel()
        ModuleNetworkStackClient.getInstance(mContext).makeDhcpServer(TEST_IFNAME, testParams,
                mDhcpServerCb)
        verify(mConnector).makeDhcpServer(TEST_IFNAME, testParams, mDhcpServerCb)
    }

    @Test
    fun testNetworkMonitorAvailableImmediately() {
        NetworkStack.setServiceForTest(mConnectorBinder)
        val testName = "NetworkMonitorName"
        ModuleNetworkStackClient.getInstance(mContext).makeNetworkMonitor(TEST_NETWORK, testName,
                mNetworkMonitorCb)
        verify(mConnector).makeNetworkMonitor(TEST_NETWORK, testName, mNetworkMonitorCb)
    }

    @Test
    fun testIpMemoryStoreAvailableImmediately() {
        NetworkStack.setServiceForTest(mConnectorBinder)
        ModuleNetworkStackClient.getInstance(mContext).fetchIpMemoryStore(mIpMemoryStoreCb)
        verify(mConnector).fetchIpMemoryStore(mIpMemoryStoreCb)
    }
}