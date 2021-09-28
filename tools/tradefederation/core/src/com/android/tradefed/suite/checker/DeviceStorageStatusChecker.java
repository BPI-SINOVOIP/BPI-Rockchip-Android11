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
package com.android.tradefed.suite.checker;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

/**
 * Check if device has enough disk space for the given partitions.
 */
public class DeviceStorageStatusChecker implements ISystemStatusChecker {
    @Option(
        name = "minimal-storage-bytes",
        description = "Number of bytes that should be left free on the device."
    )
    private long mMinimalStorage = 50L * 1024 * 1024; // 50MB

    @Option(
        name = "partition",
        description = "Partition needed to be checked on the device."
    )
    private Set<String> mPartitions = new HashSet<>();

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult preExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        List<String> noEnoughStorage = new ArrayList<>();
        for (String partition : mPartitions) {
            long freeSpace = device.getPartitionFreeSpace(partition) * 1024;
            String message = String.format(
                    "%s bytes left on the partition %s", freeSpace, partition);
            CLog.i(message);
            if (freeSpace < mMinimalStorage) {
                noEnoughStorage.add(message);
            }
        }
        if (noEnoughStorage.isEmpty()) {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        } else {
            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            String errorMessage =
                    String.format(
                            "No enough storage left on the device in pre-execution: %s\n",
                            String.join(",", noEnoughStorage));
            CLog.e(errorMessage);
            result.setErrorMessage(errorMessage);
            return result;
        }
    }
}
