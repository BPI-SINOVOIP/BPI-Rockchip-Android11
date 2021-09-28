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

import android.os.Build
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkInfo
import android.net.NetworkInfo.DetailedState
import android.net.NetworkInfo.State
import android.telephony.TelephonyManager
import androidx.test.filters.SmallTest
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.testutils.DevSdkIgnoreRule
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.runner.RunWith
import org.junit.Test

const val TYPE_MOBILE = ConnectivityManager.TYPE_MOBILE
const val TYPE_WIFI = ConnectivityManager.TYPE_WIFI
const val MOBILE_TYPE_NAME = "mobile"
const val WIFI_TYPE_NAME = "WIFI"
const val LTE_SUBTYPE_NAME = "LTE"

@SmallTest
@RunWith(AndroidJUnit4::class)
class NetworkInfoTest {
    @Rule @JvmField
    val ignoreRule = DevSdkIgnoreRule()

    @Test
    fun testAccessNetworkInfoProperties() {
        val cm = InstrumentationRegistry.getInstrumentation().context
                .getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val ni = cm.getAllNetworkInfo()
        assertTrue(ni.isNotEmpty())

        for (netInfo in ni) {
            when (netInfo.getType()) {
                TYPE_MOBILE -> assertNetworkInfo(netInfo, MOBILE_TYPE_NAME)
                TYPE_WIFI -> assertNetworkInfo(netInfo, WIFI_TYPE_NAME)
                // TODO: Add BLUETOOTH_TETHER testing
            }
        }
    }

    private fun assertNetworkInfo(netInfo: NetworkInfo, expectedTypeName: String) {
        assertTrue(expectedTypeName.equals(netInfo.getTypeName(), ignoreCase = true))
        assertNotNull(netInfo.toString())

        if (!netInfo.isConnectedOrConnecting()) return

        assertTrue(netInfo.isAvailable())
        if (State.CONNECTED == netInfo.getState()) {
            assertTrue(netInfo.isConnected())
        }
        assertTrue(State.CONNECTING == netInfo.getState() ||
                State.CONNECTED == netInfo.getState())
        assertTrue(DetailedState.SCANNING == netInfo.getDetailedState() ||
                DetailedState.CONNECTING == netInfo.getDetailedState() ||
                DetailedState.AUTHENTICATING == netInfo.getDetailedState() ||
                DetailedState.CONNECTED == netInfo.getDetailedState())
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    fun testConstructor() {
        val networkInfo = NetworkInfo(TYPE_MOBILE, TelephonyManager.NETWORK_TYPE_LTE,
                MOBILE_TYPE_NAME, LTE_SUBTYPE_NAME)

        assertEquals(TYPE_MOBILE, networkInfo.type)
        assertEquals(TelephonyManager.NETWORK_TYPE_LTE, networkInfo.subtype)
        assertEquals(MOBILE_TYPE_NAME, networkInfo.typeName)
        assertEquals(LTE_SUBTYPE_NAME, networkInfo.subtypeName)
        assertEquals(DetailedState.IDLE, networkInfo.detailedState)
        assertEquals(State.UNKNOWN, networkInfo.state)
        assertNull(networkInfo.reason)
        assertNull(networkInfo.extraInfo)

        try {
            NetworkInfo(ConnectivityManager.MAX_NETWORK_TYPE + 1,
                    TelephonyManager.NETWORK_TYPE_LTE, MOBILE_TYPE_NAME, LTE_SUBTYPE_NAME)
            fail("Unexpected behavior. Network type is invalid.")
        } catch (e: IllegalArgumentException) {
            // Expected behavior.
        }
    }

    @Test
    fun testSetDetailedState() {
        val networkInfo = NetworkInfo(TYPE_MOBILE, TelephonyManager.NETWORK_TYPE_LTE,
                MOBILE_TYPE_NAME, LTE_SUBTYPE_NAME)
        val reason = "TestNetworkInfo"
        val extraReason = "setDetailedState test"

        networkInfo.setDetailedState(DetailedState.CONNECTED, reason, extraReason)
        assertEquals(DetailedState.CONNECTED, networkInfo.detailedState)
        assertEquals(State.CONNECTED, networkInfo.state)
        assertEquals(reason, networkInfo.reason)
        assertEquals(extraReason, networkInfo.extraInfo)
    }
}
