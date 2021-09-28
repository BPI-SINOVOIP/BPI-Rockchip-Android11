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

package com.android.networkstack.netlink.util

import android.net.DnsResolver
import android.net.DnsResolver.FLAG_EMPTY
import android.net.DnsResolver.TYPE_A
import android.net.DnsResolver.TYPE_AAAA
import android.net.Network
import com.android.testutils.FakeDns
import androidx.test.filters.SmallTest
import androidx.test.runner.AndroidJUnit4
import com.android.networkstack.util.DnsUtils
import com.android.networkstack.util.DnsUtils.TYPE_ADDRCONFIG
import com.android.server.connectivity.NetworkMonitor.DnsLogFunc
import java.net.InetAddress
import java.net.UnknownHostException
import kotlin.test.assertFailsWith
import org.junit.Assert.assertArrayEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations

const val DEFAULT_TIMEOUT_MS = 1000
const val SHORT_TIMEOUT_MS = 200

@RunWith(AndroidJUnit4::class)
@SmallTest
class DnsUtilsTest {
    val fakeNetwork: Network = Network(1234)
    @Mock
    lateinit var mockLogger: DnsLogFunc
    @Mock
    lateinit var mockResolver: DnsResolver
    lateinit var fakeDns: FakeDns

    @Before
    fun setup() {
        MockitoAnnotations.initMocks(this)
        fakeDns = FakeDns(mockResolver)
        fakeDns.startMocking()
    }

    private fun assertIpAddressArrayEquals(expect: Array<String>, actual: Array<InetAddress>) =
            assertArrayEquals("Array of IP addresses differs", expect,
                    actual.map { it.getHostAddress() }.toTypedArray())

    @Test
    fun testGetAllByNameWithTypeSuccess() {
        // Test different query types.
        verifyGetAllByName("www.google.com", arrayOf("2001:db8::1"), TYPE_AAAA)
        verifyGetAllByName("www.google.com", arrayOf("192.168.0.1"), TYPE_A)
        verifyGetAllByName("www.android.com", arrayOf("192.168.0.2", "2001:db8::2"),
                TYPE_ADDRCONFIG)
    }

    private fun verifyGetAllByName(name: String, expected: Array<String>, type: Int) {
        fakeDns.setAnswer(name, expected, type)
        DnsUtils.getAllByName(mockResolver, fakeNetwork, name, type, FLAG_EMPTY, DEFAULT_TIMEOUT_MS,
                mockLogger).let { assertIpAddressArrayEquals(expected, it) }
    }

    @Test
    fun testGetAllByNameWithTypeNoResult() {
        verifyGetAllByNameFails("www.android.com", TYPE_A)
        verifyGetAllByNameFails("www.android.com", TYPE_AAAA)
        verifyGetAllByNameFails("www.android.com", TYPE_ADDRCONFIG)
    }

    private fun verifyGetAllByNameFails(name: String, type: Int) {
        assertFailsWith<UnknownHostException> {
            DnsUtils.getAllByName(mockResolver, fakeNetwork, name, type,
                    FLAG_EMPTY, SHORT_TIMEOUT_MS, mockLogger)
        }
    }
    // TODO: Add more tests. Verify timeout, logger and error.
}