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

package com.android.server

import android.content.Context
import android.net.IIpMemoryStoreCallbacks
import android.net.INetd
import android.net.INetworkMonitorCallbacks
import android.net.INetworkStackConnector
import android.net.InetAddresses.parseNumericAddress
import android.net.Network
import android.net.dhcp.DhcpServer
import android.net.dhcp.DhcpServingParamsParcel
import android.net.dhcp.IDhcpServer
import android.net.dhcp.IDhcpServerCallbacks
import android.net.ip.IIpClientCallbacks
import android.net.ip.IpClient
import android.os.Build
import android.os.IBinder
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.SmallTest
import com.android.net.module.util.Inet4AddressUtils.inet4AddressToIntHTH
import com.android.server.NetworkStackService.Dependencies
import com.android.server.NetworkStackService.NetworkStackConnector
import com.android.server.NetworkStackService.PermissionChecker
import com.android.server.connectivity.NetworkMonitor
import com.android.server.connectivity.ipmemorystore.IpMemoryStoreService
import com.android.testutils.DevSdkIgnoreRule
import com.android.testutils.DevSdkIgnoreRule.IgnoreAfter
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.eq
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.FileDescriptor
import java.io.PrintWriter
import java.io.StringWriter
import java.net.Inet4Address
import kotlin.reflect.KVisibility
import kotlin.reflect.full.declaredMemberFunctions
import kotlin.test.assertEquals

private val TEST_NETD_VERSION = 9991001
private val TEST_NETD_HASH = "test_netd_hash"

private val TEST_IFACE = "test_iface"

@RunWith(AndroidJUnit4::class)
@SmallTest
class NetworkStackServiceTest {
    @Rule @JvmField
    val ignoreRule = DevSdkIgnoreRule()

    private val permChecker = mock(PermissionChecker::class.java)
    private val mockIpMemoryStoreService = mock(IpMemoryStoreService::class.java)
    private val mockDhcpServer = mock(DhcpServer::class.java)
    private val mockNetworkMonitor = mock(NetworkMonitor::class.java)
    private val mockIpClient = mock(IpClient::class.java)
    private val deps = mock(Dependencies::class.java).apply {
        doReturn(mockIpMemoryStoreService).`when`(this).makeIpMemoryStoreService(any())
        doReturn(mockDhcpServer).`when`(this).makeDhcpServer(any(), any(), any(), any())
        doReturn(mockNetworkMonitor).`when`(this).makeNetworkMonitor(any(), any(), any(), any(),
                any())
        doReturn(mockIpClient).`when`(this).makeIpClient(any(), any(), any(), any(), any())
    }
    private val netd = mock(INetd::class.java).apply {
        doReturn(TEST_NETD_VERSION).`when`(this).interfaceVersion
        doReturn(TEST_NETD_HASH).`when`(this).interfaceHash
    }
    private val netdBinder = mock(IBinder::class.java).apply {
        doReturn(netd).`when`(this).queryLocalInterface(any())
    }
    private val context = mock(Context::class.java).apply {
        doReturn(netdBinder).`when`(this).getSystemService(Context.NETD_SERVICE)
    }

    private val connector = NetworkStackConnector(context, permChecker, deps)

    @Test @IgnoreAfter(Build.VERSION_CODES.Q)
    fun testDumpVersion_Q() {
        prepareDumpVersionTest()

        val dumpsysOut = StringWriter()
        connector.dump(FileDescriptor(), PrintWriter(dumpsysOut, true /* autoFlush */),
                arrayOf("version") /* args */)

        assertEquals("NetworkStack version:\n" +
                "NetworkStackConnector: ${INetworkStackConnector.VERSION}\n" +
                "SystemServer: {9990001, 9990002, 9990003, 9990004, 9990005}\n" +
                "Netd: $TEST_NETD_VERSION\n\n",
                dumpsysOut.toString())
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    fun testDumpVersion() {
        prepareDumpVersionTest()

        val connectorVersion = INetworkStackConnector.VERSION
        val connectorHash = INetworkStackConnector.HASH

        val dumpsysOut = StringWriter()
        connector.dump(FileDescriptor(), PrintWriter(dumpsysOut, true /* autoFlush */),
                arrayOf("version") /* args */)

        assertEquals("NetworkStack version:\n" +
                "LocalInterface:$connectorVersion:$connectorHash\n" +
                "ipmemorystore:9990001:ipmemorystore_hash\n" +
                "netd:$TEST_NETD_VERSION:$TEST_NETD_HASH\n" +
                "networkstack:9990002:dhcp_server_hash\n" +
                "networkstack:9990003:networkmonitor_hash\n" +
                "networkstack:9990004:ipclient_hash\n" +
                "networkstack:9990005:multiple_use_hash\n\n",
                dumpsysOut.toString())
    }

    fun prepareDumpVersionTest() {
        // Call each method on INetworkStackConnector and verify that it notes down the version of
        // the remote. This is usually a component in the system server that implements one of the
        // NetworkStack AIDL callback interfaces (e.g., IIpClientCallbacks). On a device there may
        // be different versions of the generated AIDL classes for different components, even within
        // the same process (e.g., system_server).
        // Call fetchIpMemoryStore
        val mockIpMemoryStoreCb = mock(IIpMemoryStoreCallbacks::class.java)
        doReturn(9990001).`when`(mockIpMemoryStoreCb).interfaceVersion
        doReturn("ipmemorystore_hash").`when`(mockIpMemoryStoreCb).interfaceHash

        connector.fetchIpMemoryStore(mockIpMemoryStoreCb)
        // IpMemoryStore was created at initialization time
        verify(mockIpMemoryStoreCb).onIpMemoryStoreFetched(any())

        // Call makeDhcpServer
        val testParams = DhcpServingParamsParcel()
        testParams.linkMtu = 1500
        testParams.dhcpLeaseTimeSecs = 3600L
        testParams.serverAddr = inet4AddressToIntHTH(
                parseNumericAddress("192.168.1.1") as Inet4Address)
        testParams.serverAddrPrefixLength = 24

        val mockDhcpCb = mock(IDhcpServerCallbacks::class.java)
        doReturn(9990002).`when`(mockDhcpCb).interfaceVersion
        doReturn("dhcp_server_hash").`when`(mockDhcpCb).interfaceHash

        connector.makeDhcpServer(TEST_IFACE, testParams, mockDhcpCb)

        verify(deps).makeDhcpServer(any(), eq(TEST_IFACE), any(), any())
        verify(mockDhcpCb).onDhcpServerCreated(eq(IDhcpServer.STATUS_SUCCESS), any())

        // Call makeNetworkMonitor
        // Use a spy of INetworkMonitorCallbacks and not a mock, as mockito can't create a mock on Q
        // because of the missing CaptivePortalData class that is an argument of one of the methods
        val mockNetworkMonitorCb = spy(INetworkMonitorCallbacks.Stub.asInterface(
                mock(IBinder::class.java)))
        doReturn(9990003).`when`(mockNetworkMonitorCb).interfaceVersion
        doReturn("networkmonitor_hash").`when`(mockNetworkMonitorCb).interfaceHash

        connector.makeNetworkMonitor(Network(123), "test_nm", mockNetworkMonitorCb)

        verify(deps).makeNetworkMonitor(any(), any(), eq(Network(123)), any(), any())
        verify(mockNetworkMonitorCb).onNetworkMonitorCreated(any())

        // Call makeIpClient
        // Use a spy of IIpClientCallbacks instead of a mock, as mockito cannot create a mock on Q
        // because of the missing CaptivePortalData class that is an argument on one of the methods
        val mockIpClientCb = mock(IIpClientCallbacks::class.java)
        doReturn(9990004).`when`(mockIpClientCb).interfaceVersion
        doReturn("ipclient_hash").`when`(mockIpClientCb).interfaceHash

        connector.makeIpClient(TEST_IFACE, mockIpClientCb)

        verify(deps).makeIpClient(any(), eq(TEST_IFACE), any(), any(), any())
        verify(mockIpClientCb).onIpClientCreated(any())

        // Call some methods one more time with a shared version number and hash to verify no
        // duplicates are reported
        doReturn(9990005).`when`(mockIpClientCb).interfaceVersion
        doReturn("multiple_use_hash").`when`(mockIpClientCb).interfaceHash
        connector.makeIpClient(TEST_IFACE, mockIpClientCb)
        verify(mockIpClientCb, times(2)).onIpClientCreated(any())

        doReturn(9990005).`when`(mockDhcpCb).interfaceVersion
        doReturn("multiple_use_hash").`when`(mockDhcpCb).interfaceHash
        connector.makeDhcpServer(TEST_IFACE, testParams, mockDhcpCb)
        verify(mockDhcpCb, times(2)).onDhcpServerCreated(eq(IDhcpServer.STATUS_SUCCESS), any())

        // Verify all methods were covered by the test (4 methods + getVersion + getHash)
        assertEquals(6, INetworkStackConnector::class.declaredMemberFunctions.count {
            it.visibility == KVisibility.PUBLIC
        })
    }
}