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
package com.android.tradefed.suite.checker;

import com.android.tradefed.device.BackgroundDeviceAction;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Status checker to ensure a module does not leak a running Thread.
 *
 * <p>TODO: Consider filtering the expected threads: main invocation and background threads and only
 * return the list of unexpected threads.
 */
public class LeakedThreadStatusChecker implements ISystemStatusChecker {

    private static final int EXPECTED_THREAD_COUNT = 1;

    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.SUCCESS);

        int numThread = Thread.currentThread().getThreadGroup().activeCount();
        if (numThread == EXPECTED_THREAD_COUNT) {
            // No stray thread detected at the end of invocation
            return result;
        }
        Thread[] listThreads = new Thread[numThread];
        Thread.currentThread().getThreadGroup().enumerate(listThreads);

        List<Thread> arrayThread = new ArrayList<>(Arrays.asList(listThreads));
        // We might have a device background action running for logcat, exclude it from the count
        // if it's one of the two threads.
        if (numThread == 2) {
            for (Thread t : arrayThread) {
                if (t.getName().contains(BackgroundDeviceAction.BACKGROUND_DEVICE_ACTION)) {
                    return result;
                }
            }
        }

        result.setStatus(CheckStatus.FAILED);
        String message =
                String.format("We have %s threads instead of 1. List: %s", numThread, arrayThread);
        result.setErrorMessage(message);
        CLog.e(message);
        return result;
    }
}
