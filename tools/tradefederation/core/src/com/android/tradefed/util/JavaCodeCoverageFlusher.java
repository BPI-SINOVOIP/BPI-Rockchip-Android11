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

package com.android.tradefed.util;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;

import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * A utility class that resets and forces a flush of Java code coverage measurements from processes
 * running on the device.
 */
public class JavaCodeCoverageFlusher {

    private static final String COVERAGE_RESET_FORMAT =
            "am attach-agent %s /system/lib/libdumpcoverage.so=reset";
    private static final String COVERAGE_FLUSH_FORMAT =
            "am attach-agent %s /system/lib/libdumpcoverage.so=dump:%s";
    private static final String PACKAGE_PREFIX = "package:";

    private final ITestDevice mDevice;
    private final List<String> mProcessNames;

    public JavaCodeCoverageFlusher(ITestDevice device, List<String> processNames) {
        mDevice = device;
        mProcessNames = ImmutableList.copyOf(processNames);
    }

    /** Retrieves the name of all running processes on the device. */
    private List<String> getAllProcessNames() throws DeviceNotAvailableException {
        List<ProcessInfo> processes = PsParser.getProcesses(mDevice.executeShellCommand("ps -e"));
        List<String> names = new ArrayList<>();

        for (ProcessInfo pinfo : processes) {
            names.add(pinfo.getName());
        }

        return names;
    }

    /** Retrieves the set of all installed packages on the device. */
    private Set<String> getAllPackages() throws DeviceNotAvailableException {
        Splitter splitter = Splitter.on('\n').trimResults().omitEmptyStrings();
        String pmListPackages = mDevice.executeShellCommand("pm list packages -a");

        List<String> lines = splitter.splitToList(pmListPackages);
        ImmutableSet.Builder<String> packages = ImmutableSet.builder();

        // Strip "package:" prefix from the output.
        for (String line : lines) {
            if (line.startsWith(PACKAGE_PREFIX)) {
                line = line.substring(PACKAGE_PREFIX.length());
            }

            packages.add(line);
        }

        return packages.build();
    }

    /**
     * Resets the Java code coverage counters for the given processes.
     *
     * @throws DeviceNotAvailableException
     */
    public void resetCoverage() throws DeviceNotAvailableException {
        List<String> processNames = mProcessNames;

        if (processNames.isEmpty()) {
            processNames = getAllProcessNames();
        }

        Set<String> javaPackages = getAllPackages();

        // Attempt to reset the coverage measurements for the given processes.
        for (String processName :
                Sets.intersection(javaPackages, ImmutableSet.copyOf(processNames))) {
            String pid = mDevice.getProcessPid(processName);

            if (pid != null) {
                mDevice.executeShellCommand(String.format(COVERAGE_RESET_FORMAT, processName));
            }
        }

        // Reset coverage for system_server.
        mDevice.executeShellCommand("cmd coverage reset");
    }

    /**
     * Forces a flush of Java coverage data from processes running on the device.
     *
     * @return a list of the coverage files generated on the device.
     * @throws DeviceNotAvailableException
     */
    public List<String> forceCoverageFlush() throws DeviceNotAvailableException {
        List<String> processNames = mProcessNames;
        if (processNames.isEmpty()) {
            processNames = getAllProcessNames();
        }

        List<String> coverageFiles = new ArrayList<>();

        Set<String> javaPackages = getAllPackages();

        // Flush coverage for the given processes, if the process exists and is a Java process.
        for (String processName :
                Sets.intersection(javaPackages, ImmutableSet.copyOf(processNames))) {
            String pid = mDevice.getProcessPid(processName);

            if (pid != null) {
                String outputPath = String.format("/data/misc/trace/%s-%s.ec", processName, pid);
                mDevice.executeShellCommand(
                        String.format(COVERAGE_FLUSH_FORMAT, processName, outputPath));
                coverageFiles.add(outputPath);
            }
        }

        // Flush coverage for system_server.
        mDevice.executeShellCommand("cmd coverage dump /data/misc/trace/system_server.ec");
        coverageFiles.add("/data/misc/trace/system_server.ec");

        return coverageFiles;
    }
}
