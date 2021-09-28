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
package com.android.tradefed.cluster;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.device.DeviceManager;
import com.android.tradefed.device.DeviceManager.FastbootDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.VersionParser;

import com.google.common.base.Strings;
import com.google.common.net.HostAndPort;
import com.google.common.net.InetAddresses;
import com.google.common.primitives.Longs;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.InvalidParameterException;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Static util functions for TF Cluster to get global config instances, host information, etc. */
public class ClusterHostUtil {

    private static String sHostName = null;

    static final String DEFAULT_TF_VERSION = "(unknown)";

    private static long sTfStartTime = getCurrentTimeMillis();

    /**
     * Gets the hostname.
     *
     * @return the hostname or null if we were unable to fetch it
     */
    public static String getHostName() {
        if (sHostName == null) {
            try {
                sHostName = InetAddress.getLocalHost().getHostName();
            } catch (UnknownHostException e) {
                CLog.w("failed to get hostname: %s", e);
            }
        }
        return sHostName;
    }

    /**
     * Gets the TF version running on this host.
     *
     * @return this host's TF version.
     */
    public static String getTfVersion() {
        final String version = VersionParser.fetchVersion();
        return toValidTfVersion(version);
    }

    /**
     * Validates a TF version and returns it if it is OK.
     *
     * @param version The string for a TF version provided by {@link VersionParser}
     * @return the version if valid or a default if not.
     */
    protected static String toValidTfVersion(String version) {
        if (Strings.isNullOrEmpty(version) || Longs.tryParse(version) == null) {
            // Making sure the version is valid. It should be a build number
            return DEFAULT_TF_VERSION;
        }
        return version;
    }

    /**
     * Returns the run target for a given device descriptor.
     *
     * @param device {@link DeviceDescriptor} to get run target for.
     * @return run target.
     */
    public static String getRunTarget(
            DeviceDescriptor device, String runTargetFormat, Map<String, String> deviceTags) {
        if (runTargetFormat != null) {
            // Make sure the pattern is non-greedy.
            Pattern p = Pattern.compile("\\{([^:\\}]+)(:.*)?\\}");
            Matcher m = p.matcher(runTargetFormat);
            StringBuffer sb = new StringBuffer();
            while (m.find()) {
                String pattern = m.group(1);
                String key = null;
                String txt = null;
                switch (pattern) {
                    case "PRODUCT":
                        txt = device.getProduct();
                        break;
                    case "PRODUCT_OR_DEVICE_CLASS":
                        // TODO: Refactor the logic to handle more flexible combinations.
                        txt = device.getProduct();
                        if (device.isStubDevice()) {
                            String deviceClass = device.getDeviceClass();
                            // If it's a fastboot device we report it as the product
                            if (!FastbootDevice.class.getSimpleName().equals(deviceClass)) {
                                txt = deviceClass;
                            }
                        }
                        break;
                    case "PRODUCT_VARIANT":
                        txt = device.getProductVariant();
                        break;
                    case "API_LEVEL":
                        txt = device.getSdkVersion();
                        break;
                    case "DEVICE_CLASS":
                        txt = device.getDeviceClass();
                        break;
                    case "SERIAL":
                        txt = device.getSerial();
                        break;
                    case "TAG":
                        if (deviceTags == null || deviceTags.isEmpty()) {
                            // simply delete the placeholder if there's nothing to match
                            txt = "";
                        } else {
                            txt = deviceTags.get(device.getSerial());
                            if (txt == null) {
                                txt = ""; // simply delete it if a tag does not exist
                            }
                        }
                        break;
                    case "DEVICE_PROP":
                        key = m.group(2).substring(1);
                        txt = device.getProperty(key);
                        break;
                    default:
                        throw new InvalidParameterException(
                                String.format(
                                        "Unsupported pattern '%s' found for run target '%s'",
                                        pattern, runTargetFormat));
                }
                if (txt == null || DeviceManager.UNKNOWN_DISPLAY_STRING.equals(txt)) {
                    return DeviceManager.UNKNOWN_DISPLAY_STRING;
                }
                m.appendReplacement(sb, Matcher.quoteReplacement(txt));
            }
            m.appendTail(sb);
            return sb.toString();
        }
        // Default behavior.
        // TODO: Remove this when we cluster default run target is changed.
        String runTarget = device.getProduct();
        if (!runTarget.equals(device.getProductVariant())) {
            runTarget += ":" + device.getProductVariant();
        }
        return runTarget;
    }

    /**
     * Checks if a given input is a valid IP:PORT string.
     *
     * @param input a string to check
     * @return true if the given input is an IP:PORT string
     */
    public static boolean isIpPort(String input) {
        try {
            HostAndPort hostAndPort = HostAndPort.fromString(input);
            return InetAddresses.isInetAddress(hostAndPort.getHost());
        } catch (IllegalArgumentException e) {
            return false;
        }
    }

    /**
     * Returns the current system time.
     *
     * @return time in millis.
     */
    public static long getCurrentTimeMillis() {
        return System.currentTimeMillis();
    }

    public static long getTfStartTimeMillis() {
        return sTfStartTime;
    }

    /** Get the {@link IClusterOptions} instance used to store cluster-related settings. */
    public static IClusterOptions getClusterOptions() {
        IClusterOptions clusterOptions =
                (IClusterOptions)
                        GlobalConfiguration.getInstance()
                                .getConfigurationObject(ClusterOptions.TYPE_NAME);
        if (clusterOptions == null) {
            throw new IllegalStateException(
                    "cluster_options not defined. You must add this "
                            + "object to your global config. See google/atp/cluster.xml.");
        }

        return clusterOptions;
    }

    /** Get the {@link IClusterClient} instance used to interact with the TFC backend. */
    public static IClusterClient getClusterClient() {
        IClusterClient ClusterClient =
                (IClusterClient)
                        GlobalConfiguration.getInstance()
                                .getConfigurationObject(IClusterClient.TYPE_NAME);
        if (ClusterClient == null) {
            throw new IllegalStateException(
                    "cluster_client not defined. You must add this "
                            + "object to your global config. See google/atp/cluster.xml.");
        }

        return ClusterClient;
    }
}
