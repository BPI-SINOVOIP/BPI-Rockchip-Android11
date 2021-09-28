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

package com.android.networkstack.metrics

import android.net.NetworkCapabilities.TRANSPORT_CELLULAR
import android.net.captiveportal.CaptivePortalProbeResult
import android.net.metrics.ValidationProbeEvent
import android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_DNS
import android.telephony.TelephonyManager
import androidx.test.filters.SmallTest
import androidx.test.runner.AndroidJUnit4
import com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession
import com.android.dx.mockito.inline.extended.ExtendedMockito.verify
import com.android.server.connectivity.nano.CellularData
import com.android.server.connectivity.nano.DataStallEventProto
import com.android.server.connectivity.nano.DnsEvent
import com.google.protobuf.nano.MessageNano
import java.util.Arrays
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq

@RunWith(AndroidJUnit4::class)
@SmallTest
class DataStallStatsUtilsTest {
    private val TEST_ELAPSED_TIME_MS = 123456789L
    private val TEST_MCCMNC = "123456"
    private val TEST_SIGNAL_STRENGTH = -100
    private val RETURN_CODE_DNS_TIMEOUT = 255

    @Test
    fun testProbeResultToEnum() {
        assertEquals(DataStallStatsUtils.probeResultToEnum(null), DataStallEventProto.INVALID)
        // Metrics cares only http response code.
        assertEquals(DataStallStatsUtils.probeResultToEnum(
                CaptivePortalProbeResult.failed(ValidationProbeEvent.PROBE_HTTP)),
                DataStallEventProto.INVALID)
        assertEquals(DataStallStatsUtils.probeResultToEnum(
                CaptivePortalProbeResult.failed(ValidationProbeEvent.PROBE_HTTPS)),
                DataStallEventProto.INVALID)
        assertEquals(DataStallStatsUtils.probeResultToEnum(
                CaptivePortalProbeResult.success(ValidationProbeEvent.PROBE_HTTP)),
                DataStallEventProto.VALID)
        assertEquals(DataStallStatsUtils.probeResultToEnum(
                CaptivePortalProbeResult.success(ValidationProbeEvent.PROBE_HTTP)),
                DataStallEventProto.VALID)
        assertEquals(DataStallStatsUtils.probeResultToEnum(CaptivePortalProbeResult.PARTIAL),
                DataStallEventProto.PARTIAL)
        assertEquals(DataStallStatsUtils.probeResultToEnum(CaptivePortalProbeResult(
                CaptivePortalProbeResult.PORTAL_CODE, ValidationProbeEvent.PROBE_HTTP)),
                DataStallEventProto.PORTAL)
        assertEquals(DataStallStatsUtils.probeResultToEnum(CaptivePortalProbeResult(
                CaptivePortalProbeResult.PORTAL_CODE, ValidationProbeEvent.PROBE_HTTPS)),
                DataStallEventProto.PORTAL)
    }

    @Test
    fun testWrite() {
        val session = mockitoSession().spyStatic(NetworkStackStatsLog::class.java).startMocking()
        val stats = DataStallDetectionStats.Builder()
                .setEvaluationType(DATA_STALL_EVALUATION_TYPE_DNS)
                .setNetworkType(TRANSPORT_CELLULAR)
                .setCellData(TelephonyManager.NETWORK_TYPE_LTE /* radioType */,
                        true /* roaming */,
                        TEST_MCCMNC /* networkMccmnc */,
                        TEST_MCCMNC /* simMccmnc */,
                        TEST_SIGNAL_STRENGTH /* signalStrength */)
                .setTcpFailRate(90)
                .setTcpSentSinceLastRecv(10)
        generateTimeoutDnsEvent(stats, count = 5)
        DataStallStatsUtils.write(stats.build(), CaptivePortalProbeResult.PARTIAL)

        verify { NetworkStackStatsLog.write(
                eq(NetworkStackStatsLog.DATA_STALL_EVENT),
                eq(DATA_STALL_EVALUATION_TYPE_DNS),
                eq(DataStallEventProto.PARTIAL),
                eq(TRANSPORT_CELLULAR),
                eq(DataStallDetectionStats.emptyWifiInfoIfNull(null)),
                eq(makeTestCellDataNano()),
                eq(makeTestDnsTimeoutNano(5)),
                eq(90) /* tcpFailRate */,
                eq(10) /* tcpSentSinceLastRecv */) }

        session.finishMocking()
    }

    private fun makeTestDnsTimeoutNano(timeoutCount: Int): ByteArray? {
        // Make an expected nano dns message.
        val event = DnsEvent()
        event.dnsReturnCode = IntArray(timeoutCount)
        event.dnsTime = LongArray(timeoutCount)
        Arrays.fill(event.dnsReturnCode, RETURN_CODE_DNS_TIMEOUT)
        Arrays.fill(event.dnsTime, TEST_ELAPSED_TIME_MS)
        return MessageNano.toByteArray(event)
    }

    private fun makeTestCellDataNano(): ByteArray? {
        // Make an expected nano cell data message.
        val data = CellularData()
        data.ratType = DataStallEventProto.RADIO_TECHNOLOGY_LTE
        data.networkMccmnc = TEST_MCCMNC
        data.simMccmnc = TEST_MCCMNC
        data.isRoaming = true
        data.signalStrength = TEST_SIGNAL_STRENGTH
        return MessageNano.toByteArray(data)
    }

    private fun generateTimeoutDnsEvent(stats: DataStallDetectionStats.Builder, count: Int) {
        repeat(count) {
            stats.addDnsEvent(RETURN_CODE_DNS_TIMEOUT, TEST_ELAPSED_TIME_MS)
        }
    }
}
