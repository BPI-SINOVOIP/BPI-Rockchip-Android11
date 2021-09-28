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

package android.net.util;

import android.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Collection of utilities for data stall.
 */
public class DataStallUtils {
    public static final int DATA_STALL_EVALUATION_TYPE_NONE = 0;
    /** Detect data stall using dns timeout counts. */
    public static final int DATA_STALL_EVALUATION_TYPE_DNS = 1 << 0;
    /** Detect data stall using tcp connection fail rate. */
    public static final int DATA_STALL_EVALUATION_TYPE_TCP = 1 << 1;

    @IntDef(prefix = { "DATA_STALL_EVALUATION_TYPE_" }, value = {
        DATA_STALL_EVALUATION_TYPE_NONE,
        DATA_STALL_EVALUATION_TYPE_DNS,
        DATA_STALL_EVALUATION_TYPE_TCP,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EvaluationType {
    }

    // Default configuration values for data stall detection.
    public static final int DEFAULT_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD = 5;
    public static final int DEFAULT_DATA_STALL_MIN_EVALUATE_TIME_MS = 60 * 1000;
    public static final int DEFAULT_DATA_STALL_VALID_DNS_TIME_THRESHOLD_MS = 30 * 60 * 1000;
    /**
     * The threshold value for the number of consecutive dns timeout events received to be a
     * signal of data stall. The number of consecutive timeouts needs to be {@code >=} this
     * threshold to be considered a data stall. Set the value to {@code <= 0} to disable. Note
     * that the value should be {@code > 0} if the DNS data stall detection is enabled.
     *
     */
    public static final String CONFIG_DATA_STALL_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD =
            "data_stall_consecutive_dns_timeout_threshold";

    /**
     * The minimal time interval in milliseconds for data stall reevaluation.
     *
     */
    public static final String CONFIG_DATA_STALL_MIN_EVALUATE_INTERVAL =
            "data_stall_min_evaluate_interval";

    /**
     * DNS timeouts older than this timeout (in milliseconds) are not considered for detecting
     * a data stall.
     *
     */
    public static final String CONFIG_DATA_STALL_VALID_DNS_TIME_THRESHOLD =
            "data_stall_valid_dns_time_threshold";

    /**
     * Which data stall detection signal to use. This is a bitmask constructed by bitwise-or-ing
     * (i.e. {@code |}) the DATA_STALL_EVALUATION_TYPE_* values.
     *
     * Type: int
     * Valid values:
     *   {@link #DATA_STALL_EVALUATION_TYPE_DNS} : Use dns as a signal.
     *   {@link #DATA_STALL_EVALUATION_TYPE_TCP} : Use tcp info as a signal.
     */
    public static final String CONFIG_DATA_STALL_EVALUATION_TYPE = "data_stall_evaluation_type";
    public static final int DEFAULT_DATA_STALL_EVALUATION_TYPES =
            DATA_STALL_EVALUATION_TYPE_DNS | DATA_STALL_EVALUATION_TYPE_TCP;
    // The default number of DNS events kept of the log kept for dns signal evaluation. Each event
    // is represented by a {@link com.android.server.connectivity.NetworkMonitor#DnsResult} objects.
    // It's also the size of array of {@link com.android.server.connectivity.nano.DnsEvent} kept in
    // metrics. Note that increasing the size may cause statsd log buffer bust. Need to check the
    // design in statsd when you try to increase the size.
    public static final int DEFAULT_DNS_LOG_SIZE = 20;

    /**
     * The time interval for polling tcp info to observe the tcp health.
     */
    public static String CONFIG_DATA_STALL_TCP_POLLING_INTERVAL = "data_stall_tcp_polling_interval";

    /**
     * Default polling interval to observe the tcp health.
     */
    public static int DEFAULT_TCP_POLLING_INTERVAL_MS = 20_000;

    /**
     * Default tcp packets fail rate to suspect as a data stall.
     *
     * Calculated by ((# of packets lost)+(# of packets retrans))/(# of packets sent)*100. Ideally,
     * the percentage should be 100%. However, the ongoing packets may not be considered as neither
     * lost or retrans yet. It will cause the percentage lower.
     */
    public static final int DEFAULT_TCP_PACKETS_FAIL_PERCENTAGE = 80;

    /**
     * The percentage of tcp packets fail rate to be suspected as a data stall.
     *
     * Type: int
     * Valid values: 0 to 100.
     */
    public static final String CONFIG_TCP_PACKETS_FAIL_PERCENTAGE = "tcp_packets_fail_percentage";

    /** Corresponds to enum from bionic/libc/include/netinet/tcp.h. */
    public static final int TCP_ESTABLISHED = 1;
    public static final int TCP_SYN_SENT = 2;
    public static final int TCP_SYN_RECV = 3;
    public static final int TCP_MONITOR_STATE_FILTER =
            (1 << TCP_ESTABLISHED) | (1 << TCP_SYN_SENT) | (1 << TCP_SYN_RECV);

    /**
     * Threshold for the minimal tcp packets count to evaluate data stall via tcp info.
     */
    public static final int DEFAULT_DATA_STALL_MIN_PACKETS_THRESHOLD = 10;
    public static final String CONFIG_MIN_PACKETS_THRESHOLD = "tcp_min_packets_threshold";
}
