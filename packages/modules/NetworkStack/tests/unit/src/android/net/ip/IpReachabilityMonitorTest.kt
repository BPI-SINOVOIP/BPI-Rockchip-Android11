/*
 * Copyright (C) 2017 The Android Open Source Project
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
package android.net.ip

import android.content.Context
import android.net.INetd
import android.net.InetAddresses.parseNumericAddress
import android.net.IpPrefix
import android.net.LinkAddress
import android.net.LinkProperties
import android.net.RouteInfo
import android.net.metrics.IpConnectivityLog
import android.net.netlink.StructNdMsg.NUD_FAILED
import android.net.netlink.StructNdMsg.NUD_STALE
import android.net.netlink.makeNewNeighMessage
import android.net.util.InterfaceParams
import android.net.util.SharedLog
import android.os.Handler
import android.os.HandlerThread
import android.os.MessageQueue
import android.os.MessageQueue.OnFileDescriptorEventListener
import android.system.ErrnoException
import android.system.OsConstants.EAGAIN
import androidx.test.filters.SmallTest
import androidx.test.runner.AndroidJUnit4
import com.android.testutils.waitForIdle
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.timeout
import org.mockito.Mockito.verify
import java.io.FileDescriptor
import java.net.Inet4Address
import java.net.Inet6Address
import java.net.InetAddress
import java.util.concurrent.CompletableFuture
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.TimeUnit
import kotlin.test.assertTrue
import kotlin.test.fail

private const val TEST_TIMEOUT_MS = 10_000L

private val TEST_IPV4_GATEWAY = parseNumericAddress("192.168.222.3") as Inet4Address
private val TEST_IPV6_GATEWAY = parseNumericAddress("2001:db8::1") as Inet6Address

private val TEST_IPV4_LINKADDR = LinkAddress("192.168.222.123/24")
private val TEST_IPV6_LINKADDR = LinkAddress("2001:db8::123/64")

// DNSes inside IP prefix
private val TEST_IPV4_DNS = parseNumericAddress("192.168.222.1") as Inet4Address
private val TEST_IPV6_DNS = parseNumericAddress("2001:db8::321") as Inet6Address

private val TEST_IFACE = InterfaceParams("fake0", 21, null)
private val TEST_LINK_PROPERTIES = LinkProperties().apply {
    interfaceName = TEST_IFACE.name
    addLinkAddress(TEST_IPV4_LINKADDR)
    addLinkAddress(TEST_IPV6_LINKADDR)

    // Add on link routes
    addRoute(RouteInfo(TEST_IPV4_LINKADDR, null /* gateway */, TEST_IFACE.name))
    addRoute(RouteInfo(TEST_IPV6_LINKADDR, null /* gateway */, TEST_IFACE.name))

    // Add default routes
    addRoute(RouteInfo(IpPrefix(parseNumericAddress("0.0.0.0"), 0), TEST_IPV4_GATEWAY))
    addRoute(RouteInfo(IpPrefix(parseNumericAddress("::"), 0), TEST_IPV6_GATEWAY))

    addDnsServer(TEST_IPV4_DNS)
    addDnsServer(TEST_IPV6_DNS)
}

/**
 * Tests for IpReachabilityMonitor.
 */
@RunWith(AndroidJUnit4::class)
@SmallTest
class IpReachabilityMonitorTest {
    private val callback = mock(IpReachabilityMonitor.Callback::class.java)
    private val dependencies = mock(IpReachabilityMonitor.Dependencies::class.java)
    private val log = mock(SharedLog::class.java)
    private val context = mock(Context::class.java)
    private val netd = mock(INetd::class.java)
    private val fd = mock(FileDescriptor::class.java)
    private val metricsLog = mock(IpConnectivityLog::class.java)

    private val handlerThread = HandlerThread(IpReachabilityMonitorTest::class.simpleName)
    private val handler by lazy { Handler(handlerThread.looper) }

    private lateinit var reachabilityMonitor: IpReachabilityMonitor
    private lateinit var neighborMonitor: TestIpNeighborMonitor

    /**
     * A version of [IpNeighborMonitor] that overrides packet reading from a socket, and instead
     * allows the test to enqueue test packets via [enqueuePacket].
     */
    private class TestIpNeighborMonitor(
        handler: Handler,
        log: SharedLog,
        cb: NeighborEventConsumer,
        private val fd: FileDescriptor
    ) : IpNeighborMonitor(handler, log, cb) {

        private val pendingPackets = ConcurrentLinkedQueue<ByteArray>()
        val msgQueue = mock(MessageQueue::class.java)

        private var eventListener: OnFileDescriptorEventListener? = null

        override fun createFd() = fd
        override fun getMessageQueue() = msgQueue

        fun enqueuePacket(packet: ByteArray) {
            val listener = eventListener ?: fail("IpNeighborMonitor was not yet started")
            pendingPackets.add(packet)
            handler.post {
                listener.onFileDescriptorEvents(fd, OnFileDescriptorEventListener.EVENT_INPUT)
            }
        }

        override fun readPacket(fd: FileDescriptor, packetBuffer: ByteArray): Int {
            val packet = pendingPackets.poll() ?: throw ErrnoException("No pending packet", EAGAIN)
            if (packet.size > packetBuffer.size) {
                fail("Buffer (${packetBuffer.size}) is too small for packet (${packet.size})")
            }
            System.arraycopy(packet, 0, packetBuffer, 0, packet.size)
            return packet.size
        }

        override fun onStart() {
            super.onStart()

            // Find the file descriptor listener that was registered on the instrumented queue
            val captor = ArgumentCaptor.forClass(OnFileDescriptorEventListener::class.java)
            verify(msgQueue).addOnFileDescriptorEventListener(
                    eq(fd), anyInt(), captor.capture())
            eventListener = captor.value
        }
    }

    @Before
    fun setUp() {
        doReturn(log).`when`(log).forSubComponent(anyString())
        doReturn(true).`when`(fd).valid()
        handlerThread.start()

        doAnswer { inv ->
            val handler = inv.getArgument<Handler>(0)
            val log = inv.getArgument<SharedLog>(1)
            val cb = inv.getArgument<IpNeighborMonitor.NeighborEventConsumer>(2)
            neighborMonitor = TestIpNeighborMonitor(handler, log, cb, fd)
            neighborMonitor
        }.`when`(dependencies).makeIpNeighborMonitor(any(), any(), any())

        val monitorFuture = CompletableFuture<IpReachabilityMonitor>()
        // IpReachabilityMonitor needs to be started from the handler thread
        handler.post {
            monitorFuture.complete(IpReachabilityMonitor(
                    context,
                    TEST_IFACE,
                    handler,
                    log,
                    callback,
                    false /* useMultinetworkPolicyTracker */,
                    dependencies,
                    metricsLog,
                    netd))
        }
        reachabilityMonitor = monitorFuture.get(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        assertTrue(::neighborMonitor.isInitialized,
                "IpReachabilityMonitor did not call makeIpNeighborMonitor")
    }

    @After
    fun tearDown() {
        // Ensure the handler thread is not accessing the fd while changing its mock
        handlerThread.waitForIdle(TEST_TIMEOUT_MS)
        doReturn(false).`when`(fd).valid()
        handlerThread.quitSafely()
    }

    @Test
    fun testLoseProvisioning_FirstProbeIsFailed() {
        reachabilityMonitor.updateLinkProperties(TEST_LINK_PROPERTIES)

        neighborMonitor.enqueuePacket(makeNewNeighMessage(TEST_IPV4_DNS, NUD_FAILED))
        verify(callback, timeout(TEST_TIMEOUT_MS)).notifyLost(eq(TEST_IPV4_DNS), anyString())
    }

    private fun runLoseProvisioningTest(lostNeighbor: InetAddress) {
        reachabilityMonitor.updateLinkProperties(TEST_LINK_PROPERTIES)

        neighborMonitor.enqueuePacket(makeNewNeighMessage(TEST_IPV4_GATEWAY, NUD_STALE))
        neighborMonitor.enqueuePacket(makeNewNeighMessage(TEST_IPV6_GATEWAY, NUD_STALE))
        neighborMonitor.enqueuePacket(makeNewNeighMessage(TEST_IPV4_DNS, NUD_STALE))
        neighborMonitor.enqueuePacket(makeNewNeighMessage(TEST_IPV6_DNS, NUD_STALE))

        neighborMonitor.enqueuePacket(makeNewNeighMessage(lostNeighbor, NUD_FAILED))
        verify(callback, timeout(TEST_TIMEOUT_MS)).notifyLost(eq(lostNeighbor), anyString())
    }

    @Test
    fun testLoseProvisioning_Ipv4DnsLost() {
        runLoseProvisioningTest(TEST_IPV4_DNS)
    }

    @Test
    fun testLoseProvisioning_Ipv6DnsLost() {
        runLoseProvisioningTest(TEST_IPV6_DNS)
    }

    @Test
    fun testLoseProvisioning_Ipv4GatewayLost() {
        runLoseProvisioningTest(TEST_IPV4_GATEWAY)
    }

    @Test
    fun testLoseProvisioning_Ipv6GatewayLost() {
        runLoseProvisioningTest(TEST_IPV6_GATEWAY)
    }
}