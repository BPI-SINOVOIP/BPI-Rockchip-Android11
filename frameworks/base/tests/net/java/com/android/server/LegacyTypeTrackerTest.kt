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

package com.android.server

import android.net.ConnectivityManager.TYPE_ETHERNET
import android.net.ConnectivityManager.TYPE_MOBILE
import android.net.ConnectivityManager.TYPE_MOBILE_SUPL
import android.net.ConnectivityManager.TYPE_WIFI
import android.net.ConnectivityManager.TYPE_WIMAX
import android.net.NetworkInfo.DetailedState.CONNECTED
import android.net.NetworkInfo.DetailedState.DISCONNECTED
import androidx.test.filters.SmallTest
import androidx.test.runner.AndroidJUnit4
import com.android.server.ConnectivityService.LegacyTypeTracker
import com.android.server.connectivity.NetworkAgentInfo
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

const val UNSUPPORTED_TYPE = TYPE_WIMAX

@RunWith(AndroidJUnit4::class)
@SmallTest
class LegacyTypeTrackerTest {
    private val supportedTypes = arrayOf(TYPE_MOBILE, TYPE_WIFI, TYPE_ETHERNET, TYPE_MOBILE_SUPL)

    private val mMockService = mock(ConnectivityService::class.java).apply {
        doReturn(false).`when`(this).isDefaultNetwork(any())
    }
    private val mTracker = LegacyTypeTracker(mMockService).apply {
        supportedTypes.forEach {
            addSupportedType(it)
        }
    }

    @Test
    fun testSupportedTypes() {
        try {
            mTracker.addSupportedType(supportedTypes[0])
            fail("Expected IllegalStateException")
        } catch (expected: IllegalStateException) {}
        supportedTypes.forEach {
            assertTrue(mTracker.isTypeSupported(it))
        }
        assertFalse(mTracker.isTypeSupported(UNSUPPORTED_TYPE))
    }

    @Test
    fun testSupl() {
        val mobileNai = mock(NetworkAgentInfo::class.java)
        mTracker.add(TYPE_MOBILE, mobileNai)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, CONNECTED, TYPE_MOBILE)
        reset(mMockService)
        mTracker.add(TYPE_MOBILE_SUPL, mobileNai)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, CONNECTED, TYPE_MOBILE_SUPL)
        reset(mMockService)
        mTracker.remove(TYPE_MOBILE_SUPL, mobileNai, false /* wasDefault */)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, DISCONNECTED, TYPE_MOBILE_SUPL)
        reset(mMockService)
        mTracker.add(TYPE_MOBILE_SUPL, mobileNai)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, CONNECTED, TYPE_MOBILE_SUPL)
        reset(mMockService)
        mTracker.remove(mobileNai, false)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, DISCONNECTED, TYPE_MOBILE_SUPL)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai, DISCONNECTED, TYPE_MOBILE)
    }

    @Test
    fun testAddNetwork() {
        val mobileNai = mock(NetworkAgentInfo::class.java)
        val wifiNai = mock(NetworkAgentInfo::class.java)
        mTracker.add(TYPE_MOBILE, mobileNai)
        mTracker.add(TYPE_WIFI, wifiNai)
        assertSame(mTracker.getNetworkForType(TYPE_MOBILE), mobileNai)
        assertSame(mTracker.getNetworkForType(TYPE_WIFI), wifiNai)
        // Make sure adding a second NAI does not change the results.
        val secondMobileNai = mock(NetworkAgentInfo::class.java)
        mTracker.add(TYPE_MOBILE, secondMobileNai)
        assertSame(mTracker.getNetworkForType(TYPE_MOBILE), mobileNai)
        assertSame(mTracker.getNetworkForType(TYPE_WIFI), wifiNai)
        // Make sure removing a network that wasn't added for this type is a no-op.
        mTracker.remove(TYPE_MOBILE, wifiNai, false /* wasDefault */)
        assertSame(mTracker.getNetworkForType(TYPE_MOBILE), mobileNai)
        assertSame(mTracker.getNetworkForType(TYPE_WIFI), wifiNai)
        // Remove the top network for mobile and make sure the second one becomes the network
        // of record for this type.
        mTracker.remove(TYPE_MOBILE, mobileNai, false /* wasDefault */)
        assertSame(mTracker.getNetworkForType(TYPE_MOBILE), secondMobileNai)
        assertSame(mTracker.getNetworkForType(TYPE_WIFI), wifiNai)
        // Make sure adding a network for an unsupported type does not register it.
        mTracker.add(UNSUPPORTED_TYPE, mobileNai)
        assertNull(mTracker.getNetworkForType(UNSUPPORTED_TYPE))
    }

    @Test
    fun testBroadcastOnDisconnect() {
        val mobileNai1 = mock(NetworkAgentInfo::class.java)
        val mobileNai2 = mock(NetworkAgentInfo::class.java)
        doReturn(false).`when`(mMockService).isDefaultNetwork(mobileNai1)
        mTracker.add(TYPE_MOBILE, mobileNai1)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai1, CONNECTED, TYPE_MOBILE)
        reset(mMockService)
        doReturn(false).`when`(mMockService).isDefaultNetwork(mobileNai2)
        mTracker.add(TYPE_MOBILE, mobileNai2)
        verify(mMockService, never()).sendLegacyNetworkBroadcast(any(), any(), anyInt())
        mTracker.remove(TYPE_MOBILE, mobileNai1, false /* wasDefault */)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai1, DISCONNECTED, TYPE_MOBILE)
        verify(mMockService).sendLegacyNetworkBroadcast(mobileNai2, CONNECTED, TYPE_MOBILE)
    }
}
