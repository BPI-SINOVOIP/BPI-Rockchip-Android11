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

package android.net.cts

import android.Manifest.permission.MANAGE_TEST_NETWORKS
import android.Manifest.permission.NETWORK_SETTINGS
import android.content.Context
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.EthernetManager
import android.net.InetAddresses
import android.net.NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL
import android.net.NetworkCapabilities.NET_CAPABILITY_TRUSTED
import android.net.NetworkCapabilities.TRANSPORT_ETHERNET
import android.net.NetworkCapabilities.TRANSPORT_TEST
import android.net.NetworkRequest
import android.net.TestNetworkInterface
import android.net.TestNetworkManager
import android.net.Uri
import android.net.dhcp.DhcpDiscoverPacket
import android.net.dhcp.DhcpPacket
import android.net.dhcp.DhcpPacket.DHCP_MESSAGE_TYPE
import android.net.dhcp.DhcpPacket.DHCP_MESSAGE_TYPE_DISCOVER
import android.net.dhcp.DhcpPacket.DHCP_MESSAGE_TYPE_REQUEST
import android.net.dhcp.DhcpRequestPacket
import android.os.Build
import android.os.HandlerThread
import android.platform.test.annotations.AppModeFull
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity
import com.android.compatibility.common.util.ThrowingRunnable
import com.android.net.module.util.Inet4AddressUtils.getBroadcastAddress
import com.android.net.module.util.Inet4AddressUtils.getPrefixMaskAsInet4Address
import com.android.server.util.NetworkStackConstants.IPV4_ADDR_ANY
import com.android.testutils.DevSdkIgnoreRule
import com.android.testutils.DhcpClientPacketFilter
import com.android.testutils.DhcpOptionFilter
import com.android.testutils.RecorderCallback.CallbackEntry
import com.android.testutils.TapPacketReader
import com.android.testutils.TestableNetworkCallback
import fi.iki.elonen.NanoHTTPD
import org.junit.After
import org.junit.Assume.assumeFalse
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.net.Inet4Address
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.TimeUnit
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.test.fail

private const val MAX_PACKET_LENGTH = 1500
private const val TEST_TIMEOUT_MS = 10_000L

private const val TEST_LEASE_TIMEOUT_SECS = 3600 * 12
private const val TEST_PREFIX_LENGTH = 24

private const val TEST_LOGIN_URL = "https://login.capport.android.com"
private const val TEST_VENUE_INFO_URL = "https://venueinfo.capport.android.com"
private const val TEST_DOMAIN_NAME = "lan"
private const val TEST_MTU = 1500.toShort()

@AppModeFull(reason = "Instant apps cannot create test networks")
@RunWith(AndroidJUnit4::class)
class CaptivePortalApiTest {
    @JvmField
    @Rule
    val ignoreRule = DevSdkIgnoreRule(ignoreClassUpTo = Build.VERSION_CODES.Q)

    private val context by lazy { InstrumentationRegistry.getInstrumentation().context }
    private val tnm by lazy { context.assertHasService(TestNetworkManager::class.java) }
    private val eth by lazy { context.assertHasService(EthernetManager::class.java) }
    private val cm by lazy { context.assertHasService(ConnectivityManager::class.java) }

    private val handlerThread = HandlerThread(CaptivePortalApiTest::class.java.simpleName)
    private val serverIpAddr = InetAddresses.parseNumericAddress("192.0.2.222") as Inet4Address
    private val clientIpAddr = InetAddresses.parseNumericAddress("192.0.2.111") as Inet4Address
    private val httpServer = HttpServer()
    private val ethRequest = NetworkRequest.Builder()
            // ETHERNET|TEST transport networks do not have NET_CAPABILITY_TRUSTED
            .removeCapability(NET_CAPABILITY_TRUSTED)
            .addTransportType(TRANSPORT_ETHERNET)
            .addTransportType(TRANSPORT_TEST).build()
    private val ethRequestCb = TestableNetworkCallback()

    private lateinit var iface: TestNetworkInterface
    private lateinit var reader: TapPacketReader
    private lateinit var capportUrl: Uri

    private var testSkipped = false

    @Before
    fun setUp() {
        // This test requires using a tap interface as an ethernet interface.
        val pm = context.getPackageManager()
        testSkipped = !pm.hasSystemFeature(PackageManager.FEATURE_ETHERNET) &&
                context.getSystemService(EthernetManager::class.java) == null
        assumeFalse(testSkipped)

        // Register a request so the network does not get torn down
        cm.requestNetwork(ethRequest, ethRequestCb)
        runAsShell(NETWORK_SETTINGS, MANAGE_TEST_NETWORKS) {
            eth.setIncludeTestInterfaces(true)
            // Keeping a reference to the test interface also makes sure the ParcelFileDescriptor
            // does not go out of scope, which would cause it to close the underlying FileDescriptor
            // in its finalizer.
            iface = tnm.createTapInterface()
        }

        handlerThread.start()
        reader = TapPacketReader(
                handlerThread.threadHandler,
                iface.fileDescriptor.fileDescriptor,
                MAX_PACKET_LENGTH)
        handlerThread.threadHandler.post { reader.start() }
        httpServer.start()

        // Pad the listening port to make sure it is always of length 5. This ensures the URL has
        // always the same length so the test can use constant IP and UDP header lengths.
        // The maximum port number is 65535 so a length of 5 is always enough.
        capportUrl = Uri.parse("http://localhost:${httpServer.listeningPort}/testapi.html?par=val")
    }

    @After
    fun tearDown() {
        if (testSkipped) return
        cm.unregisterNetworkCallback(ethRequestCb)

        runAsShell(NETWORK_SETTINGS) { eth.setIncludeTestInterfaces(false) }

        httpServer.stop()
        handlerThread.threadHandler.post { reader.stop() }
        handlerThread.quitSafely()

        iface.fileDescriptor.close()
    }

    @Test
    fun testApiCallbacks() {
        // Handle the DHCP handshake that includes the capport API URL
        val discover = reader.assertDhcpPacketReceived(
                DhcpDiscoverPacket::class.java, TEST_TIMEOUT_MS, DHCP_MESSAGE_TYPE_DISCOVER)
        reader.sendResponse(makeOfferPacket(discover.clientMac, discover.transactionId))

        val request = reader.assertDhcpPacketReceived(
                DhcpRequestPacket::class.java, TEST_TIMEOUT_MS, DHCP_MESSAGE_TYPE_REQUEST)
        assertEquals(discover.transactionId, request.transactionId)
        assertEquals(clientIpAddr, request.mRequestedIp)
        reader.sendResponse(makeAckPacket(request.clientMac, request.transactionId))

        // Expect a request to the capport API
        val capportReq = httpServer.recordedRequests.poll(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        assertNotNull(capportReq, "The device did not fetch captive portal API data within timeout")
        assertEquals(capportUrl.path, capportReq.uri)
        assertEquals(capportUrl.query, capportReq.queryParameterString)

        // Expect network callbacks with capport info
        val testCb = TestableNetworkCallback(TEST_TIMEOUT_MS)
        // LinkProperties do not contain captive portal info if the callback is registered without
        // NETWORK_SETTINGS permissions.
        val lp = runAsShell(NETWORK_SETTINGS) {
            cm.registerNetworkCallback(ethRequest, testCb)

            try {
                val ncCb = testCb.eventuallyExpect<CallbackEntry.CapabilitiesChanged> {
                    it.caps.hasCapability(NET_CAPABILITY_CAPTIVE_PORTAL)
                }
                testCb.eventuallyExpect<CallbackEntry.LinkPropertiesChanged> {
                    it.network == ncCb.network && it.lp.captivePortalData != null
                }.lp
            } finally {
                cm.unregisterNetworkCallback(testCb)
            }
        }

        assertEquals(capportUrl, lp.captivePortalApiUrl)
        with(lp.captivePortalData) {
            assertNotNull(this)
            assertTrue(isCaptive)
            assertEquals(Uri.parse(TEST_LOGIN_URL), userPortalUrl)
            assertEquals(Uri.parse(TEST_VENUE_INFO_URL), venueInfoUrl)
        }
    }

    private fun makeOfferPacket(clientMac: ByteArray, transactionId: Int) =
            DhcpPacket.buildOfferPacket(DhcpPacket.ENCAP_L2, transactionId,
                    false /* broadcast */, serverIpAddr, IPV4_ADDR_ANY /* relayIp */, clientIpAddr,
                    clientMac, TEST_LEASE_TIMEOUT_SECS,
                    getPrefixMaskAsInet4Address(TEST_PREFIX_LENGTH),
                    getBroadcastAddress(clientIpAddr, TEST_PREFIX_LENGTH),
                    listOf(serverIpAddr) /* gateways */, listOf(serverIpAddr) /* dnsServers */,
                    serverIpAddr, TEST_DOMAIN_NAME, null /* hostname */, true /* metered */,
                    TEST_MTU, capportUrl.toString())

    private fun makeAckPacket(clientMac: ByteArray, transactionId: Int) =
            DhcpPacket.buildAckPacket(DhcpPacket.ENCAP_L2, transactionId,
                    false /* broadcast */, serverIpAddr, IPV4_ADDR_ANY /* relayIp */, clientIpAddr,
                    clientIpAddr /* requestClientIp */, clientMac, TEST_LEASE_TIMEOUT_SECS,
                    getPrefixMaskAsInet4Address(TEST_PREFIX_LENGTH),
                    getBroadcastAddress(clientIpAddr, TEST_PREFIX_LENGTH),
                    listOf(serverIpAddr) /* gateways */, listOf(serverIpAddr) /* dnsServers */,
                    serverIpAddr, TEST_DOMAIN_NAME, null /* hostname */, true /* metered */,
                    TEST_MTU, false /* rapidCommit */, capportUrl.toString())

    private fun parseDhcpPacket(bytes: ByteArray) = DhcpPacket.decodeFullPacket(
            bytes, MAX_PACKET_LENGTH, DhcpPacket.ENCAP_L2)
}

/**
 * A minimal HTTP server running on localhost (loopback), on a random available port.
 *
 * The server records each request in [recordedRequests] and will not serve any further request
 * until the last one is removed from the queue for verification.
 */
private class HttpServer : NanoHTTPD("localhost", 0 /* auto-select the port */) {
    val recordedRequests = ArrayBlockingQueue<IHTTPSession>(1 /* capacity */)

    override fun serve(session: IHTTPSession): Response {
        recordedRequests.offer(session)
        return newFixedLengthResponse("""
                |{
                |  "captive": true,
                |  "user-portal-url": "$TEST_LOGIN_URL",
                |  "venue-info-url": "$TEST_VENUE_INFO_URL"
                |}
            """.trimMargin())
    }
}

private fun <T : DhcpPacket> TapPacketReader.assertDhcpPacketReceived(
    packetType: Class<T>,
    timeoutMs: Long,
    type: Byte
): T {
    val packetBytes = popPacket(timeoutMs, DhcpClientPacketFilter()
            .and(DhcpOptionFilter(DHCP_MESSAGE_TYPE, type)))
            ?: fail("${packetType.simpleName} not received within timeout")
    val packet = DhcpPacket.decodeFullPacket(packetBytes, packetBytes.size, DhcpPacket.ENCAP_L2)
    assertTrue(packetType.isInstance(packet),
            "Expected ${packetType.simpleName} but got ${packet.javaClass.simpleName}")
    return packetType.cast(packet)
}

private fun <T> Context.assertHasService(manager: Class<T>): T {
    return getSystemService(manager) ?: fail("Service $manager not found")
}

/**
 * Wrapper around runWithShellPermissionIdentity with kotlin-like syntax.
 */
private fun <T> runAsShell(vararg permissions: String, task: () -> T): T {
    var ret: T? = null
    runWithShellPermissionIdentity(ThrowingRunnable { ret = task() }, *permissions)
    return ret ?: fail("ThrowingRunnable was not run")
}
