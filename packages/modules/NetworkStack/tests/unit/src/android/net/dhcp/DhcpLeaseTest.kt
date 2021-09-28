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

package android.net.dhcp

import android.net.InetAddresses.parseNumericAddress
import android.net.MacAddress
import com.android.net.module.util.Inet4AddressUtils.intToInet4AddressHTH
import androidx.test.filters.SmallTest
import androidx.test.runner.AndroidJUnit4
import com.android.testutils.assertFieldCountEquals
import org.junit.Assert.assertArrayEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.net.Inet4Address
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals

@RunWith(AndroidJUnit4::class)
@SmallTest
class DhcpLeaseTest {
    companion object {
        private val TEST_CLIENT_ID = byteArrayOf(0, 1, 2, 127)
        private val TEST_HWADDR = MacAddress.fromString("01:23:45:67:8F:9A")
        private val TEST_INETADDR = parseNumericAddress("192.168.42.123") as Inet4Address
        private val TEST_PREFIXLEN = 23
        private val TEST_EXPTIME = 1234L
        private val TEST_HOSTNAME = "test_hostname"
    }

    @Test
    fun testToParcelable() {
        val lease = DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, TEST_HOSTNAME)

        assertParcelEquals(lease, lease.toParcelable())
    }

    @Test
    fun testToParcelable_NullFields() {
        val lease = DhcpLease(null, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN, TEST_EXPTIME, null)
        assertParcelEquals(lease, lease.toParcelable())
    }

    @Test
    fun testEquals() {
        val lease = DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, TEST_HOSTNAME)
        assertEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, TEST_HOSTNAME))

        // Change client ID
        assertNotEquals(lease, DhcpLease(null, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, TEST_HOSTNAME))
        assertNotEquals(lease, DhcpLease(byteArrayOf(42), TEST_HWADDR, TEST_INETADDR,
                TEST_PREFIXLEN, TEST_EXPTIME, TEST_HOSTNAME))

        // Change mac address
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, MacAddress.fromString("12:34:56:78:9A:0B"),
                TEST_INETADDR, TEST_PREFIXLEN, TEST_EXPTIME, TEST_HOSTNAME))

        // Change address
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR,
                parseNumericAddress("192.168.43.43") as Inet4Address, TEST_PREFIXLEN, TEST_EXPTIME,
                TEST_HOSTNAME))

        // Change prefix length
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, 24,
                TEST_EXPTIME, TEST_HOSTNAME))

        // Change expiry time
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                4567L, TEST_HOSTNAME))

        // Change hostname
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, null))
        assertNotEquals(lease, DhcpLease(TEST_CLIENT_ID, TEST_HWADDR, TEST_INETADDR, TEST_PREFIXLEN,
                TEST_EXPTIME, "other_hostname"))

        assertFieldCountEquals(6, DhcpLease::class.java)
    }

    private fun assertParcelEquals(expected: DhcpLease, p: DhcpLeaseParcelable) {
        assertArrayEquals(expected.clientId, p.clientId)
        assertEquals(expected.hwAddr, MacAddress.fromBytes(p.hwAddr))
        assertEquals(expected.netAddr, intToInet4AddressHTH(p.netAddr))
        assertEquals(expected.prefixLength, p.prefixLength)
        assertEquals(expected.expTime, p.expTime)
        assertEquals(expected.hostname, p.hostname)

        assertFieldCountEquals(6, DhcpLease::class.java)
    }
}