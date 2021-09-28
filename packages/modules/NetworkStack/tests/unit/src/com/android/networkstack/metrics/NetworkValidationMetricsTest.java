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

package com.android.networkstack.metrics;

import static android.net.NetworkCapabilities.TRANSPORT_BLUETOOTH;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_ETHERNET;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;

import static org.junit.Assert.assertTrue;

import android.net.INetworkMonitor;
import android.net.NetworkCapabilities;
import android.net.captiveportal.CaptivePortalProbeResult;
import android.net.metrics.ValidationProbeEvent;
import android.stats.connectivity.ProbeResult;
import android.stats.connectivity.ProbeType;
import android.stats.connectivity.TransportType;
import android.stats.connectivity.ValidationResult;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.networkstack.apishim.CaptivePortalDataShimImpl;
import com.android.networkstack.apishim.common.CaptivePortalDataShim;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class NetworkValidationMetricsTest {
    private static final String TEST_LOGIN_URL = "https://testportal.example.com/login";
    private static final String TEST_VENUE_INFO_URL = "https://venue.example.com/info";
    private static final int TTL_TOLERANCE_SECS = 10;

    private static final NetworkCapabilities WIFI_CAPABILITIES =
            new NetworkCapabilities()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI);

    @Test
    public void testNetworkValidationMetrics_VerifyProbeTypeToEnum() throws Exception {
        verifyProbeType(ValidationProbeEvent.PROBE_DNS, ProbeType.PT_DNS);
        verifyProbeType(ValidationProbeEvent.PROBE_HTTP, ProbeType.PT_HTTP);
        verifyProbeType(ValidationProbeEvent.PROBE_HTTPS, ProbeType.PT_HTTPS);
        verifyProbeType(ValidationProbeEvent.PROBE_PAC, ProbeType.PT_PAC);
        verifyProbeType(ValidationProbeEvent.PROBE_FALLBACK, ProbeType.PT_FALLBACK);
        verifyProbeType(ValidationProbeEvent.PROBE_PRIVDNS, ProbeType.PT_PRIVDNS);
    }

    private void verifyProbeType(int inputProbeType, ProbeType expectedEnumType) {
        assertEquals(expectedEnumType, NetworkValidationMetrics.probeTypeToEnum(inputProbeType));
    }

    @Test
    public void testNetworkValidationMetrics_VerifyHttpProbeResultToEnum() throws Exception {
        verifyProbeType(new CaptivePortalProbeResult(CaptivePortalProbeResult.SUCCESS_CODE,
                ValidationProbeEvent.PROBE_HTTP), ProbeResult.PR_SUCCESS);
        verifyProbeType(new CaptivePortalProbeResult(CaptivePortalProbeResult.FAILED_CODE,
                ValidationProbeEvent.PROBE_HTTP), ProbeResult.PR_FAILURE);
        verifyProbeType(new CaptivePortalProbeResult(CaptivePortalProbeResult.PORTAL_CODE,
                ValidationProbeEvent.PROBE_HTTP), ProbeResult.PR_PORTAL);
        verifyProbeType(CaptivePortalProbeResult.PRIVATE_IP, ProbeResult.PR_PRIVATE_IP_DNS);
    }

    private void verifyProbeType(CaptivePortalProbeResult inputResult,
            ProbeResult expectedResult) {
        assertEquals(expectedResult, NetworkValidationMetrics.httpProbeResultToEnum(inputResult));
    }

    @Test
    public void testNetworkValidationMetrics_VerifyValidationResultToEnum() throws Exception {
        verifyProbeType(INetworkMonitor.NETWORK_VALIDATION_RESULT_VALID, null,
                ValidationResult.VR_SUCCESS);
        verifyProbeType(0, TEST_LOGIN_URL, ValidationResult.VR_PORTAL);
        verifyProbeType(INetworkMonitor.NETWORK_VALIDATION_RESULT_PARTIAL, null,
                ValidationResult.VR_PARTIAL);
        verifyProbeType(0, null, ValidationResult.VR_FAILURE);
    }

    private void verifyProbeType(int inputResult, String inputRedirectUrl,
            ValidationResult expectedResult) {
        assertEquals(expectedResult, NetworkValidationMetrics.validationResultToEnum(inputResult,
                inputRedirectUrl));
    }

    @Test
    public void testNetworkValidationMetrics_VerifyTransportTypeToEnum() throws Exception {
        final NetworkValidationMetrics metrics = new NetworkValidationMetrics();
        NetworkCapabilities nc = new NetworkCapabilities();
        nc.addTransportType(TRANSPORT_WIFI);
        assertEquals(TransportType.TT_WIFI, metrics.getTransportTypeFromNC(nc));
        nc.addTransportType(TRANSPORT_VPN);
        assertEquals(TransportType.TT_WIFI_VPN, metrics.getTransportTypeFromNC(nc));
        nc.addTransportType(TRANSPORT_CELLULAR);
        assertEquals(TransportType.TT_WIFI_CELLULAR_VPN, metrics.getTransportTypeFromNC(nc));

        nc = new NetworkCapabilities();
        nc.addTransportType(TRANSPORT_CELLULAR);
        assertEquals(TransportType.TT_CELLULAR, metrics.getTransportTypeFromNC(nc));
        nc.addTransportType(TRANSPORT_VPN);
        assertEquals(TransportType.TT_CELLULAR_VPN, metrics.getTransportTypeFromNC(nc));

        nc = new NetworkCapabilities();
        nc.addTransportType(TRANSPORT_BLUETOOTH);
        assertEquals(TransportType.TT_BLUETOOTH, metrics.getTransportTypeFromNC(nc));
        nc.addTransportType(TRANSPORT_VPN);
        assertEquals(TransportType.TT_BLUETOOTH_VPN, metrics.getTransportTypeFromNC(nc));

        nc = new NetworkCapabilities();
        nc.addTransportType(TRANSPORT_ETHERNET);
        assertEquals(TransportType.TT_ETHERNET, metrics.getTransportTypeFromNC(nc));
        nc.addTransportType(TRANSPORT_VPN);
        assertEquals(TransportType.TT_ETHERNET_VPN, metrics.getTransportTypeFromNC(nc));
    }

    @Test
    public void testNetworkValidationMetrics_VerifyConsecutiveProbeFailure() throws Exception {
        final NetworkValidationMetrics metrics = new NetworkValidationMetrics();
        metrics.startCollection(WIFI_CAPABILITIES);
        // 1. PT_DNS probe
        metrics.addProbeEvent(ProbeType.PT_DNS, 1234, ProbeResult.PR_SUCCESS, null);
        // 2. Consecutive PT_HTTP probe failure
        for (int i = 0; i < 30; i++) {
            metrics.addProbeEvent(ProbeType.PT_HTTP, 1234, ProbeResult.PR_FAILURE, null);
        }

        // Write metric into statsd
        final NetworkValidationReported stats = metrics.maybeStopCollectionAndSend();

        // The maximum number of probe records should be the same as MAX_PROBE_EVENTS_COUNT
        final ProbeEvents probeEvents = stats.getProbeEvents();
        assertEquals(NetworkValidationMetrics.MAX_PROBE_EVENTS_COUNT,
                probeEvents.getProbeEventCount());
    }

    @Test
    public void testNetworkValidationMetrics_VerifyCollectMetrics() throws Exception {
        final long bytesRemaining = 12_345L;
        final long secondsRemaining = 3000L;
        String apiContent = "{'captive': true,"
                + "'user-portal-url': '" + TEST_LOGIN_URL + "',"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "',"
                + "'bytes-remaining': " + bytesRemaining + ","
                + "'seconds-remaining': " + secondsRemaining + "}";
        final NetworkValidationMetrics metrics = new NetworkValidationMetrics();
        final int validationIndex = 1;
        final long longlatency = Integer.MAX_VALUE + 12344567L;
        metrics.startCollection(WIFI_CAPABILITIES);

        final JSONObject info = new JSONObject(apiContent);
        final CaptivePortalDataShim captivePortalData = CaptivePortalDataShimImpl.isSupported()
                ? CaptivePortalDataShimImpl.fromJson(info) : null;

        // 1. PT_CAPPORT_API probe w CapportApiData info
        metrics.addProbeEvent(ProbeType.PT_CAPPORT_API, 1234, ProbeResult.PR_SUCCESS,
                captivePortalData);
        // 2. PT_CAPPORT_API probe w/o CapportApiData info
        metrics.addProbeEvent(ProbeType.PT_CAPPORT_API, 1234, ProbeResult.PR_FAILURE, null);

        // 3. PT_DNS probe
        metrics.addProbeEvent(ProbeType.PT_DNS, 5678, ProbeResult.PR_FAILURE, null);

        // 4. PT_HTTP probe
        metrics.addProbeEvent(ProbeType.PT_HTTP, longlatency, ProbeResult.PR_PORTAL, null);

        // add Validation result
        metrics.setValidationResult(INetworkMonitor.NETWORK_VALIDATION_RESULT_PARTIAL, null);

        // Write metric into statsd
        final NetworkValidationReported stats = metrics.maybeStopCollectionAndSend();

        // Verify: TransportType: WIFI
        assertEquals(TransportType.TT_WIFI, stats.getTransportType());

        // Verify: validationIndex
        assertEquals(validationIndex, stats.getValidationIndex());

        //  probe Count: 4 (PT_CAPPORT_API, PT_DNS, PT_HTTP, PT_CAPPORT_API)
        final ProbeEvents probeEvents = stats.getProbeEvents();
        assertEquals(4, probeEvents.getProbeEventCount());

        // Verify the 1st probe: ProbeType = PT_CAPPORT_API, Latency_us = 1234,
        //                       ProbeResult = PR_SUCCESS, CapportApiData = capportData
        ProbeEvent probeEvent = probeEvents.getProbeEvent(0);
        assertEquals(ProbeType.PT_CAPPORT_API, probeEvent.getProbeType());
        assertEquals(1234, probeEvent.getLatencyMicros());
        assertEquals(ProbeResult.PR_SUCCESS, probeEvent.getProbeResult());
        if (CaptivePortalDataShimImpl.isSupported()) {
            assertTrue(probeEvent.hasCapportApiData());
            // Set secondsRemaining to 3000 and check that getRemainingTtlSecs is within 10 seconds
            final CapportApiData capportData = probeEvent.getCapportApiData();
            assertTrue(capportData.getRemainingTtlSecs() <= secondsRemaining);
            assertTrue(capportData.getRemainingTtlSecs() + TTL_TOLERANCE_SECS > secondsRemaining);
            assertEquals(captivePortalData.getByteLimit() / 1000, capportData.getRemainingBytes());
        } else {
            assertFalse(probeEvent.hasCapportApiData());
        }

        // Verify the 2nd probe: ProbeType = PT_CAPPORT_API, Latency_us = 1234,
        //                       ProbeResult = PR_SUCCESS, CapportApiData = null
        probeEvent = probeEvents.getProbeEvent(1);
        assertEquals(ProbeType.PT_CAPPORT_API, probeEvent.getProbeType());
        assertEquals(1234, probeEvent.getLatencyMicros());
        assertEquals(ProbeResult.PR_FAILURE, probeEvent.getProbeResult());
        assertEquals(false, probeEvent.hasCapportApiData());

        // Verify the 3rd probe: ProbeType = PT_DNS, Latency_us = 5678,
        //                       ProbeResult = PR_FAILURE, CapportApiData = null
        probeEvent = probeEvents.getProbeEvent(2);
        assertEquals(ProbeType.PT_DNS, probeEvent.getProbeType());
        assertEquals(5678, probeEvent.getLatencyMicros());
        assertEquals(ProbeResult.PR_FAILURE, probeEvent.getProbeResult());
        assertEquals(false, probeEvent.hasCapportApiData());

        // Verify the 4th probe: ProbeType = PT_HTTP, Latency_us = longlatency,
        //                       ProbeResult = PR_PORTAL, CapportApiData = null
        probeEvent = probeEvents.getProbeEvent(3);
        assertEquals(ProbeType.PT_HTTP, probeEvent.getProbeType());
        // The latency exceeds Integer.MAX_VALUE(2147483647), it is limited to Integer.MAX_VALUE
        assertEquals(Integer.MAX_VALUE, probeEvent.getLatencyMicros());
        assertEquals(ProbeResult.PR_PORTAL, probeEvent.getProbeResult());
        assertEquals(false, probeEvent.hasCapportApiData());

        // Verify the ValidationResult
        assertEquals(ValidationResult.VR_PARTIAL, stats.getValidationResult());
    }
}
