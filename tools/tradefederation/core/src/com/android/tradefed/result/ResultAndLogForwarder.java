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
package com.android.tradefed.result;

import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;

import java.util.List;

/** Forwarder for results and logs events. */
public class ResultAndLogForwarder extends ResultForwarder implements ILogSaverListener {

    /** Ctor */
    public ResultAndLogForwarder(List<ITestInvocationListener> listeners) {
        super(listeners);
    }

    @Override
    public void invocationStarted(IInvocationContext context) {
        InvocationSummaryHelper.reportInvocationStarted(getListeners(), context);
    }

    @Override
    public void invocationEnded(long elapsedTime) {
        InvocationSummaryHelper.reportInvocationEnded(getListeners(), elapsedTime);
    }

    @Override
    public void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        try {
            for (ITestInvocationListener listener : getListeners()) {
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener)
                            .testLogSaved(dataName, dataType, dataStream, logFile);
                }
            }
        } catch (RuntimeException e) {
            CLog.e("Failed to save log data");
            CLog.e(e);
        }
    }

    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        for (ITestInvocationListener listener : getListeners()) {
            try {
                // Forward the logAssociation call
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener).logAssociation(dataName, logFile);
                }
            } catch (RuntimeException e) {
                CLog.e("Failed to provide the log association");
                CLog.e(e);
            }
        }
    }

    @Override
    public void setLogSaver(ILogSaver logSaver) {
        for (ITestInvocationListener listener : getListeners()) {
            try {
                // Forward the setLogSaver call
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener).setLogSaver(logSaver);
                }
            } catch (RuntimeException e) {
                CLog.e("Failed to setLogSaver");
                CLog.e(e);
            }
        }
    }
}
