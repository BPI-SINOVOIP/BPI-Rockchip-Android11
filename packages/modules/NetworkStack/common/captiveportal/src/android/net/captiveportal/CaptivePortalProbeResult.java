/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.net.captiveportal;

import static android.net.metrics.ValidationProbeEvent.PROBE_HTTP;
import static android.net.metrics.ValidationProbeEvent.PROBE_HTTPS;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
/**
 * Result of calling isCaptivePortal().
 * @hide
 */
public class CaptivePortalProbeResult {
    public static final int SUCCESS_CODE = 204;
    public static final int FAILED_CODE = 599;
    public static final int PORTAL_CODE = 302;
    // Set partial connectivity http response code to -1 to prevent conflict with the other http
    // response codes. Besides the default http response code of probe result is set as 599 in
    // NetworkMonitor#sendParallelHttpProbes(), so response code will be set as -1 only when
    // NetworkMonitor detects partial connectivity.
    /**
     * @hide
     */
    public static final int PARTIAL_CODE = -1;

    // DNS response with private IP on the probe URL suggests that the network, especially Wi-Fi
    // network is not connected to the Internet. This code represents the result of a single probe,
    // for correct logging of the probe results. The result of the whole evaluation would typically
    // be FAILED if one of the probes returns this status.
    // This logic is only used if the config_force_dns_probe_private_ip_no_internet flag is set.
    public static final int DNS_PRIVATE_IP_RESPONSE_CODE = -2;
    // The private IP logic only applies to the HTTP probe.
    public static final CaptivePortalProbeResult PRIVATE_IP =
            new CaptivePortalProbeResult(DNS_PRIVATE_IP_RESPONSE_CODE, 1 << PROBE_HTTP);
    // Partial connectivity should be concluded from both HTTP and HTTPS probes.
    @NonNull
    public static final CaptivePortalProbeResult PARTIAL = new CaptivePortalProbeResult(
            PARTIAL_CODE, 1 << PROBE_HTTP | 1 << PROBE_HTTPS);
    // Probe type that is used for unspecified probe result.
    public static final int PROBE_UNKNOWN = 0;

    final int mHttpResponseCode;  // HTTP response code returned from Internet probe.
    @Nullable
    public final String redirectUrl;      // Redirect destination returned from Internet probe.
    @Nullable
    public final String detectUrl;        // URL where a 204 response code indicates
                                          // captive portal has been appeased.
    @Nullable
    public final CaptivePortalProbeSpec probeSpec;

    // Indicate what kind of probe this is. This is a bitmask constructed by
    // bitwise-or-ing(i.e. {@code |}) the {@code ValidationProbeEvent.PROBE_HTTP} or
    // {@code ValidationProbeEvent.PROBE_HTTPS} values. Set it as {@code PROBE_UNKNOWN} if the probe
    // type is unspecified.
    @ProbeType
    public final int probeType;

    @IntDef(value = {
        PROBE_UNKNOWN,
        1 << PROBE_HTTP,
        1 << PROBE_HTTPS,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProbeType {
    }

    public CaptivePortalProbeResult(int httpResponseCode, @ProbeType int probeType) {
        this(httpResponseCode, null, null, null, probeType);
    }

    public CaptivePortalProbeResult(int httpResponseCode, @Nullable String redirectUrl,
            @Nullable String detectUrl, @ProbeType int probeType) {
        this(httpResponseCode, redirectUrl, detectUrl, null, probeType);
    }

    public CaptivePortalProbeResult(int httpResponseCode, @Nullable String redirectUrl,
            @Nullable String detectUrl, @Nullable CaptivePortalProbeSpec probeSpec,
            @ProbeType int probeType) {
        mHttpResponseCode = httpResponseCode;
        this.redirectUrl = redirectUrl;
        this.detectUrl = detectUrl;
        this.probeSpec = probeSpec;
        this.probeType = probeType;
    }

    public boolean isSuccessful() {
        return mHttpResponseCode == SUCCESS_CODE;
    }

    public boolean isPortal() {
        return !isSuccessful() && (mHttpResponseCode >= 200) && (mHttpResponseCode <= 399);
    }

    public boolean isFailed() {
        return !isSuccessful() && !isPortal();
    }

    public boolean isPartialConnectivity() {
        return mHttpResponseCode == PARTIAL_CODE;
    }

    public boolean isDnsPrivateIpResponse() {
        return mHttpResponseCode == DNS_PRIVATE_IP_RESPONSE_CODE;
    }

    /**
     *  Make a failed {@code this} for either HTTPS or HTTP determined by {@param isHttps}.
     *  @param probeType probe type of the result.
     *  @return a failed {@link CaptivePortalProbeResult}
     */
    public static CaptivePortalProbeResult failed(@ProbeType int probeType) {
        return new CaptivePortalProbeResult(FAILED_CODE, probeType);
    }

    /**
     *  Make a success {@code this} for either HTTPS or HTTP determined by {@param isHttps}.
     *  @param probeType probe type of the result.
     *  @return a success {@link CaptivePortalProbeResult}
     */
    public static CaptivePortalProbeResult success(@ProbeType int probeType) {
        return new CaptivePortalProbeResult(SUCCESS_CODE, probeType);
    }

    /**
     * As {@code this} is the result of calling isCaptivePortal(), the result may be determined from
     * one or more probes depending on the configuration of detection probes. Return if HTTPS probe
     * is one of the probes that concludes it.
     */
    public boolean isConcludedFromHttps() {
        return (probeType & (1 << PROBE_HTTPS)) != 0;
    }

    /**
     * As {@code this} is the result of calling isCaptivePortal(), the result may be determined from
     * one or more probes depending on the configuration of detection probes. Return if HTTP probe
     * is one of the probes that concludes it.
     */
    public boolean isConcludedFromHttp() {
        return (probeType & (1 << PROBE_HTTP)) != 0;
    }
}
