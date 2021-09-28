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
package com.android.tradefed.util.statsd;

import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.EventMetric;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.FileUtil;

import com.google.common.io.Files;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;
import java.util.stream.Collectors;

/**
 * Utility class for creating, interacting with, and pushing statsd configuration files.
 *
 * <p>TODO(b/118635164): Merge with device-side configuration utilities.
 */
public class ConfigUtil {
    private static final String REMOVE_CONFIG_CMD = "cmd stats config remove";
    private static final String UPDATE_CONFIG_CMD = "cmd stats config update";

    /**
     * Pushes an event-based configuration file to collect atoms provided in {@code eventAtomIds}.
     *
     * @param device where to push the configuration
     * @param eventAtomIds a list of event atom IDs to collect
     * @return ID of the newly pushed configuration file
     */
    public static long pushStatsConfig(ITestDevice device, List<Integer> eventAtomIds)
            throws IOException, DeviceNotAvailableException {
        StatsdConfig config = generateStatsdConfig(eventAtomIds);
        CLog.d(
                "Collecting atoms [%s] with the following config: %s",
                eventAtomIds.stream().map(String::valueOf).collect(Collectors.joining(", ")),
                config.toString());
        File configFile = null;
        try {
            configFile = File.createTempFile("statsdconfig", ".config");
            Files.write(config.toByteArray(), configFile);
            String remotePath = String.format("/data/local/tmp/%s", configFile.getName());
            if (device.pushFile(configFile, remotePath)) {
                updateConfig(device, remotePath, config.getId());
            } else {
                throw new RuntimeException("Failed to configuration push file to the device.");
            }
            return config.getId();
        } finally {
            FileUtil.deleteFile(configFile);
        }
    }

    /**
     * Pushes a binary statsd configuration file to collect metrics
     *
     * @param device Test device where the binary statsd config will be pushed to
     * @param configFile The statsd config file
     * @return ID of the newly pushed configuration file
     */
    public static long pushBinaryStatsConfig(ITestDevice device, File configFile)
            throws IOException, DeviceNotAvailableException {
        if (!configFile.exists()) {
            throw new FileNotFoundException(
                    String.format(
                            "File not found for statsd config: %s", configFile.getAbsolutePath()));
        }
        String remotePath = String.format("/data/local/tmp/%s", configFile.getName());
        if (device.pushFile(configFile, remotePath)) {
            // For now use a fixed config Id. We will parse it from config file when necessary.
            long configId = UUID.randomUUID().hashCode();
            updateConfig(device, remotePath, configId);
            return configId;
        } else {
            throw new RuntimeException("Failed to push configuration file to the device.");
        }
    }

    /**
     * Removes a statsd configuration file by it's id, {@code configId}.
     *
     * @param device where to delete the configuration
     * @param configId ID of the configuration to delete
     */
    public static void removeConfig(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        device.executeShellCommand(String.join(" ", REMOVE_CONFIG_CMD, String.valueOf(configId)));
    }

    /**
     * Push a statsd configuration from a config file on device.
     *
     * @param device Test device where the config will be uploaded
     * @param remotePath The path to the statsd config on the device
     * @param configId ID of the statsd config
     */
    private static void updateConfig(ITestDevice device, String remotePath, long configId)
            throws DeviceNotAvailableException {
        CommandResult output =
                device.executeShellV2Command(
                        String.join(
                                " ",
                                "cat",
                                remotePath,
                                "|",
                                UPDATE_CONFIG_CMD,
                                String.valueOf(configId)));
        device.deleteFile(remotePath);
        if (output.getStderr().contains("Error parsing")) {
            throw new RuntimeException("Failed to parse configuration file on the device.");
        } else if (!output.getStderr().isEmpty()) {
            throw new RuntimeException(
                    String.format("Failed to push config with error: %s.", output.getStderr()));
        }
    }

    /**
     * Creates an statsd configuration file that will collect the event atoms provided in {@code
     * eventAtomIds}. Note this only accepts data from the list of {@code #commonLogSources} now.
     *
     * @param eventAtomIds a list of event atom IDs to collect
     * @return the {@code StatsdConfig} device configuration
     */
    private static StatsdConfig generateStatsdConfig(List<Integer> eventAtomIds) {
        long configId = UUID.randomUUID().hashCode();
        StatsdConfig.Builder configBuilder =
                StatsdConfig.newBuilder()
                        .setId(configId)
                        .addAllAllowedLogSource(commonLogSources());
        // Add all event atom matchers.
        for (Integer id : eventAtomIds) {
            long atomMatcherId = UUID.randomUUID().hashCode();
            long eventMatcherId = UUID.randomUUID().hashCode();
            configBuilder =
                    configBuilder
                            .addAtomMatcher(
                                    AtomMatcher.newBuilder()
                                            .setId(atomMatcherId)
                                            .setSimpleAtomMatcher(
                                                    SimpleAtomMatcher.newBuilder().setAtomId(id)))
                            .addEventMetric(
                                    EventMetric.newBuilder()
                                            .setId(eventMatcherId)
                                            .setWhat(atomMatcherId));
        }
        return configBuilder.build();
    }

    /** Returns a list of common trusted log sources. */
    private static List<String> commonLogSources() {
        return Arrays.asList(
                "AID_BLUETOOTH",
                "AID_GRAPHICS",
                "AID_INCIENTD",
                "AID_RADIO",
                "AID_ROOT",
                "AID_STATSD",
                "AID_SYSTEM");
    }
}
